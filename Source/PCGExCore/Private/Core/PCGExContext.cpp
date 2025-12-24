// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExContext.h"

#include "Core/PCGExElement.h"
#include "Core/PCGExSettings.h"
#include "PCGComponent.h"
#include "Factories/PCGExInstancedFactory.h"
#include "PCGExCoreMacros.h"
#include "Core/PCGExMT.h"
#include "Helpers/PCGExStreamingHelpers.h"
#include "PCGManagedResource.h"
#include "Engine/AssetManager.h"
#include "Helpers/PCGHelpers.h"
#include "Async/Async.h"
#include "Containers/PCGExManagedObjects.h"
#include "Data/PCGExDataCommon.h"
#include "Engine/EngineTypes.h"
#include "Helpers/PCGDynamicTrackingHelpers.h"
#include "Helpers/PCGExFunctionPrototypes.h"
#include "Metadata/PCGMetadata.h"
#include "Utils/PCGExUniqueNameGenerator.h"

#define LOCTEXT_NAMESPACE "PCGExContext"

UPCGExInstancedFactory* FPCGExContext::RegisterOperation(UPCGExInstancedFactory* BaseOperation, const FName OverridePinLabel)
{
	BaseOperation->BindContext(this); // Temp so Copy doesn't crash

	UPCGExInstancedFactory* RetValue = BaseOperation->CreateNewInstance(ManagedObjects.Get());
	if (!RetValue) { return nullptr; }
	InternalOperations.Add(RetValue);
	RetValue->InitializeInContext(this, OverridePinLabel);
	return RetValue;
}

#pragma region Output Data

void FPCGExContext::IncreaseStagedOutputReserve(const int32 InIncreaseNum)
{
	FWriteScopeLock WriteScopeLock(StagingLock);
	StagedData.Reserve(StagedData.Num() + InIncreaseNum);
}

void FPCGExContext::StageOutput(UPCGData* InData, const FName& InPin, const PCGExData::EStaging Staging, const TSet<FString>& InTags)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExContext::StageOutput);

	if (IsWorkCancelled() || IsWorkCompleted()) { return; }

	{
		FWriteScopeLock WriteScopeLock(StagingLock);

		FPCGTaggedData& Output = StagedData.Emplace_GetRef();
		Output.Data = InData;
		Output.Pin = InPin;
		Output.Tags.Append(InTags);
		Output.bPinlessData = EnumHasAnyFlags(Staging, PCGExData::EStaging::Pinless);
	}

	if (EnumHasAnyFlags(Staging, PCGExData::EStaging::Managed)) { ManagedObjects->Add(InData); }
	if (EnumHasAnyFlags(Staging, PCGExData::EStaging::Mutable))
	{
		if (bCleanupConsumableAttributes)
		{
			if (UPCGMetadata* Metadata = InData->MutableMetadata())
			{
				for (const FName ConsumableName : ConsumableAttributesSet)
				{
					if (Metadata->HasAttribute(ConsumableName)
						&& !ProtectedAttributesSet.Contains(ConsumableName))
					{
						Metadata->DeleteAttribute(ConsumableName);
					}
				}
			}
		}

		if (bFlattenOutput) { InData->Flatten(); }
	}
}

#pragma endregion

UWorld* FPCGExContext::GetWorld() const { return GetComponent()->GetWorld(); }

const UPCGComponent* FPCGExContext::GetComponent() const
{
	return Cast<UPCGComponent>(ExecutionSource.Get());
}

UPCGComponent* FPCGExContext::GetMutableComponent() const
{
	return Cast<UPCGComponent>(ExecutionSource.Get());
}

TSharedPtr<PCGExMT::FTaskManager> FPCGExContext::GetTaskManager()
{
	if (!TaskManager)
	{
		FWriteScopeLock WriteLock(AsyncLock);
		TaskManager = MakeShared<PCGExMT::FTaskManager>(this);
		TaskManager->OnEndCallback = [CtxHandle = GetOrCreateHandle()](const bool bWasCancelled)
		{
			if (bWasCancelled) { return; }

			PCGEX_SHARED_CONTEXT_VOID(CtxHandle);

			FPCGExContext* Ctx = SharedContext.Get();

			if (Ctx && Ctx->ElementHandle) { Ctx->OnAsyncWorkEnd(bWasCancelled); }
			else { UE_LOG(LogTemp, Error, TEXT("OnEnd but no context or element handle!")) }
		};
	}

	return TaskManager;
}

void FPCGExContext::PauseContext() { bIsPaused = true; }

void FPCGExContext::UnpauseContext() { bIsPaused = false; }

FPCGExContext::FPCGExContext()
{
	WorkHandle = MakeShared<PCGEx::FWorkHandle>();
	ManagedObjects = MakeShared<PCGEx::FManagedObjects>(this, WorkHandle);
	UniqueNameGenerator = MakeShared<FPCGExUniqueNameGenerator>();
}

FPCGExContext::~FPCGExContext()
{
	//WorkHandle.Reset();
	ManagedObjects->Flush(); // So cleanups can be recursively triggered while manager is still alive
	PCGExHelpers::SafeReleaseHandles(TrackedAssets);
}

void FPCGExContext::ExecuteOnNotifyActors(const TArray<FName>& FunctionNames)
{
	// Execute PostProcess Functions
	if (!NotifyActors.IsEmpty())
	{
		if (IsInGameThread())
		{
			TArray<AActor*> NotifyActorsArray = NotifyActors.Array();
			for (AActor* TargetActor : NotifyActorsArray)
			{
				if (!IsValid(TargetActor)) { continue; }
				for (UFunction* Function : PCGExHelpers::FindUserFunctions(TargetActor->GetClass(), FunctionNames, {UPCGExFunctionPrototypes::GetPrototypeWithNoParams()}, this))
				{
					TargetActor->ProcessEvent(Function, nullptr);
				}
			}
		}
		else
		{
			PCGExMT::ExecuteOnMainThreadAndWait([&]() { ExecuteOnNotifyActors(FunctionNames); });
		}
	}
}

void FPCGExContext::AddExtraStructReferencedObjects(FReferenceCollector& Collector)
{
	FPCGContext::AddExtraStructReferencedObjects(Collector);
	ManagedObjects->AddExtraStructReferencedObjects(Collector);
}

void FPCGExContext::AddNotifyActor(AActor* InActor)
{
	if (IsValid(InActor))
	{
		FWriteScopeLock WriteScopeLock(NotifyActorsLock);
		NotifyActors.Add(InActor);
	}
}
#pragma region State

bool FPCGExContext::IsWaitingForTasks()
{
	if (TaskManager) { return TaskManager->IsWaitingForTasks(); }
	return false;
}

void FPCGExContext::ReadyForExecution()
{
	UnpauseContext();
	SetState(PCGExCommon::States::State_InitialExecution);
}

void FPCGExContext::SetState(const PCGExCommon::ContextState StateId)
{
	CurrentState.store(StateId.GetComparisonIndex().ToUnstableInt(), std::memory_order_release);
}

void FPCGExContext::Done()
{
	SetState(PCGExCommon::States::State_Done);
}

bool FPCGExContext::TryComplete(const bool bForce)
{
	if (IsWorkCancelled() || IsWorkCompleted()) { return true; }
	if (!bForce && !IsDone()) { return false; }

	bool bExpected = false;
	if (bWorkCompleted.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
	{
		OnComplete();
	}

	return true;
}

void FPCGExContext::OnAsyncWorkEnd(const bool bWasCancelled)
{
	PCGEX_SHARED_CONTEXT_VOID(GetOrCreateHandle());

	if (bWasCancelled || IsWorkCancelled()) { return; }

	// Try to become the processor
	bool bExpected = false;
	if (!bProcessingAsyncWorkEnd.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
	{
		// Someone else is processing - they'll see our pending flag
		//UE_LOG(LogTemp, Error, TEXT("Double Call : %s"), *GetNameSafe(GetInputSettings<UPCGExSettings>()))
	}

	const UPCGExSettings* Settings = GetInputSettings<UPCGExSettings>();
	switch (CurrentPhase)
	{
	case EPCGExecutionPhase::PrepareData: ElementHandle->AdvancePreparation(this, Settings);
		break;
	case EPCGExecutionPhase::Execute: ElementHandle->AdvanceWork(this, Settings);
		break;
	default: break;
	}

	bProcessingAsyncWorkEnd.store(false, std::memory_order_release);
}

void FPCGExContext::OnComplete()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExContext::OnComplete);

	//UE_LOG(LogTemp, Warning, TEXT(">> OnComplete @%s"), *GetInputSettings<UPCGExSettings>()->GetName());

	if (ElementHandle) { ElementHandle->CompleteWork(this); }

	PCGEX_TERMINATE_ASYNC

	{
		FWriteScopeLock WriteScopeLock(StagingLock);
		OutputData.TaggedData.Append(StagedData);
		ManagedObjects->Remove(StagedData);
		StagedData.Empty();
	}

	UnpauseContext();
}

#pragma endregion

#pragma region Async resource management

TSet<FSoftObjectPath>& FPCGExContext::GetRequiredAssets()
{
	FWriteScopeLock WriteScopeLock(AssetsLock);
	if (!RequiredAssets) { RequiredAssets = MakeShared<TSet<FSoftObjectPath>>(); }
	return *RequiredAssets.Get();
}

void FPCGExContext::RegisterAssetDependencies()
{
}

void FPCGExContext::AddAssetDependency(const FSoftObjectPath& Dependency)
{
	FWriteScopeLock WriteScopeLock(AssetsLock);
	if (!RequiredAssets) { RequiredAssets = MakeShared<TSet<FSoftObjectPath>>(); }
	RequiredAssets->Add(Dependency);
}

bool FPCGExContext::LoadAssets()
{
	if (!RequiredAssets || RequiredAssets->IsEmpty()) { return false; }

	SetState(PCGExCommon::States::State_LoadingAssetDependencies);

	PCGExHelpers::Load(
		GetTaskManager(),
		[CtxHandle = GetOrCreateHandle()]() -> TArray<FSoftObjectPath>
		{
			PCGEX_SHARED_CONTEXT_RET(CtxHandle, {})
			return SharedContext.Get()->RequiredAssets->Array();
		}, [CtxHandle = GetOrCreateHandle()](const bool bSuccess, TSharedPtr<FStreamableHandle> StreamableHandle)
		{
			PCGEX_SHARED_CONTEXT_VOID(CtxHandle)
			SharedContext.Get()->TrackAssetsHandle(StreamableHandle);
			if (!bSuccess) { SharedContext.Get()->CancelExecution("Error loading assets."); }
		});


	return true;
}


void FPCGExContext::TrackAssetsHandle(const TSharedPtr<FStreamableHandle>& InHandle)
{
	if (!InHandle.IsValid() || !InHandle->IsActive()) { return; }
	{
		FWriteScopeLock WriteScopeLock(AssetsLock);
		TrackedAssets.Add(InHandle);
	}
}

UPCGManagedComponent* FPCGExContext::AttachManagedComponent(AActor* InParent, UActorComponent* InComponent, const FAttachmentTransformRules& AttachmentRules) const
{
	UPCGComponent* SrcComp = GetMutableComponent();

	const bool bIsPreviewMode = SrcComp->IsInPreviewMode();

	if (!ManagedObjects->Remove(InComponent))
	{
		// If the component is not managed internally, make sure it's cleared
		InComponent->RemoveFromRoot();
		InComponent->ClearInternalFlags(EInternalObjectFlags::Async);
	}

	InComponent->ComponentTags.Reserve(InComponent->ComponentTags.Num() + 2);
	InComponent->ComponentTags.Add(SrcComp->GetFName());
	InComponent->ComponentTags.Add(PCGHelpers::DefaultPCGTag);

	UPCGManagedComponent* ManagedComponent = NewObject<UPCGManagedComponent>(SrcComp);
	ManagedComponent->GeneratedComponent = InComponent;
	SrcComp->AddToManagedResources(ManagedComponent);

	if (IPCGExManagedComponentInterface* Managed = Cast<IPCGExManagedComponentInterface>(InComponent))
	{
		Managed->SetManagedComponent(ManagedComponent);
	}

	InParent->Modify(!bIsPreviewMode);

	InComponent->RegisterComponent();
	InParent->AddInstanceComponent(InComponent);

	if (USceneComponent* SceneComponent = Cast<USceneComponent>(InComponent))
	{
		USceneComponent* Root = InParent->GetRootComponent();
		SceneComponent->SetMobility(Root->Mobility);
		SceneComponent->AttachToComponent(Root, AttachmentRules);
	}

	return ManagedComponent;
}

void FPCGExContext::AddConsumableAttributeName(const FName InName)
{
	{
		FReadScopeLock ReadScopeLock(ConsumableAttributesLock);
		if (ConsumableAttributesSet.Contains(InName)) { return; }
	}
	{
		FWriteScopeLock WriteScopeLock(ConsumableAttributesLock);
		ConsumableAttributesSet.Add(InName);
	}
}

void FPCGExContext::AddProtectedAttributeName(const FName InName)
{
	{
		FReadScopeLock ReadScopeLock(ProtectedAttributesLock);
		if (ProtectedAttributesSet.Contains(InName)) { return; }
	}
	{
		FWriteScopeLock WriteScopeLock(ProtectedAttributesLock);
		ProtectedAttributesSet.Add(InName);
	}
}

void FPCGExContext::EDITOR_TrackPath(const FSoftObjectPath& Path, const bool bIsCulled)
{
#if WITH_EDITOR
	FPCGDynamicTrackingHelper::AddSingleDynamicTrackingKey(this, FPCGSelectionKey::CreateFromPath(Path), false);
#endif
}

void FPCGExContext::EDITOR_TrackClass(const TSubclassOf<UObject>& InSelectionClass, bool bIsCulled)
{
#if WITH_EDITOR
	FPCGDynamicTrackingHelper::AddSingleDynamicTrackingKey(this, FPCGSelectionKey(InSelectionClass), false);
#endif
}

bool FPCGExContext::CanExecute() const
{
	return !InputData.bCancelExecution && !IsWorkCancelled() && !IsWorkCompleted();
}

bool FPCGExContext::CancelExecution(const FString& InReason)
{
	bool bExpected = false;
	if (bWorkCancelled.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
	{
		FSharedContext<FPCGExContext> SharedContext(GetOrCreateHandle());

		if (!bQuietCancellationError && !InReason.IsEmpty()) { PCGE_LOG_C(Error, GraphAndLog, this, FTEXT(InReason)); }

		PCGEX_TERMINATE_ASYNC

		OutputData.Reset();
		if (bPropagateAbortedExecution) { OutputData.bCancelExecution = true; }

		UnpauseContext();
	}

	return true;
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
