// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExContext.h"

#include "PCGExElement.h"
#include "PCGComponent.h"
#include "PCGExHelpers.h"
#include "PCGExInstancedFactory.h"
#include "Details/PCGExMacros.h"
#include "PCGExMT.h"
#include "PCGManagedResource.h"
#include "Engine/AssetManager.h"
#include "Helpers/PCGHelpers.h"
#include "Async/Async.h"
#include "Engine/EngineTypes.h"
#include "Helpers/PCGDynamicTrackingHelpers.h"

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
	FWriteScopeLock WriteScopeLock(StagedOutputLock);
	OutputData.TaggedData.Reserve(OutputData.TaggedData.Num() + InIncreaseNum);
}

FPCGTaggedData& FPCGExContext::StageOutput(UPCGData* InData, const bool bManaged, const bool bIsMutable)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExContext::StageOutput);

	check(WorkHandle.IsValid())

	int32 Index = -1;
	if (!IsInGameThread())
	{
		FWriteScopeLock WriteScopeLock(StagedOutputLock);

		FPCGTaggedData& Output = OutputData.TaggedData.Emplace_GetRef();
		Output.Data = InData;
		Index = OutputData.TaggedData.Num() - 1;
	}
	else
	{
		FPCGTaggedData& Output = OutputData.TaggedData.Emplace_GetRef();
		Output.Data = InData;
		Index = OutputData.TaggedData.Num() - 1;
	}

	if (bManaged) { ManagedObjects->Add(InData); }
	if (bIsMutable)
	{
		if (bCleanupConsumableAttributes)
		{
			if (UPCGMetadata* Metadata = InData->MutableMetadata())
			{
				for (const FName ConsumableName : ConsumableAttributesSet)
				{
					if (!Metadata->HasAttribute(ConsumableName) || ProtectedAttributesSet.Contains(ConsumableName)) { continue; }
					Metadata->DeleteAttribute(ConsumableName);
				}
			}
		}

		if (bFlattenOutput) { InData->Flatten(); }
	}

	return OutputData.TaggedData[Index];
}

void FPCGExContext::StageOutput(UPCGData* InData, const FName& InPin, const TSet<FString>& InTags, const bool bManaged, const bool bIsMutable, const bool bPinless)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExContext::StageOutputComplex);

	check(WorkHandle.IsValid())

	if (!IsInGameThread())
	{
		FWriteScopeLock WriteScopeLock(StagedOutputLock);

		FPCGTaggedData& Output = OutputData.TaggedData.Emplace_GetRef();
		Output.Data = InData;
		Output.Pin = InPin;
		Output.Tags.Append(InTags);
		Output.bPinlessData = bPinless;
	}
	else
	{
		FPCGTaggedData& Output = OutputData.TaggedData.Emplace_GetRef();
		Output.Data = InData;
		Output.Pin = InPin;
		Output.Tags.Append(InTags);
		Output.bPinlessData = bPinless;
	}

	if (bManaged) { ManagedObjects->Add(InData); }
	if (bIsMutable)
	{
		if (bCleanupConsumableAttributes)
		{
			if (UPCGMetadata* Metadata = InData->MutableMetadata())
			{
				for (const FName ConsumableName : ConsumableAttributesSet)
				{
					if (!Metadata->HasAttribute(ConsumableName) || ProtectedAttributesSet.Contains(ConsumableName)) { continue; }
					Metadata->DeleteAttribute(ConsumableName);
				}
			}
		}

		if (bFlattenOutput) { InData->Flatten(); }
	}
}

FPCGTaggedData& FPCGExContext::StageOutput(UPCGData* InData, const bool bManaged)
{
	check(WorkHandle.IsValid())

	int32 Index = -1;
	if (!IsInGameThread())
	{
		FWriteScopeLock WriteScopeLock(StagedOutputLock);
		FPCGTaggedData& Output = OutputData.TaggedData.Emplace_GetRef();
		Output.Data = InData;
		Index = OutputData.TaggedData.Num() - 1;
	}
	else
	{
		FPCGTaggedData& Output = OutputData.TaggedData.Emplace_GetRef();
		Output.Data = InData;
		Index = OutputData.TaggedData.Num() - 1;
	}

	if (bManaged) { ManagedObjects->Add(InData); }
	return OutputData.TaggedData[Index];
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

TSharedPtr<PCGExMT::FTaskManager> FPCGExContext::GetAsyncManager()
{
	if (!AsyncManager)
	{
		FWriteScopeLock WriteLock(AsyncLock);
		AsyncManager = MakeShared<PCGExMT::FTaskManager>(this);
		AsyncManager->OnEndCallback = [CtxHandle = GetOrCreateHandle()](const bool bWasCancelled)
		{
			if (bWasCancelled) { return; }
			
			PCGEX_SHARED_CONTEXT_VOID(CtxHandle);

			FPCGExContext* Ctx = SharedContext.Get();

			if (Ctx && Ctx->ElementHandle) { Ctx->OnAsyncWorkEnd(bWasCancelled); }
			else { UE_LOG(LogTemp, Error, TEXT("OnEnd but no context or element handle!")) }
		};

		PCGExMT::SetWorkPriority(WorkPriority, AsyncManager->WorkPriority);
	}

	return AsyncManager;
}

void FPCGExContext::PauseContext()
{
	bIsPaused = true;
}

void FPCGExContext::UnpauseContext()
{
	bIsPaused = false;
}

FPCGExContext::FPCGExContext()
{
	WorkHandle = MakeShared<PCGEx::FWorkHandle>();
	ManagedObjects = MakeShared<PCGEx::FManagedObjects>(this);
	UniqueNameGenerator = MakeShared<PCGEx::FUniqueNameGenerator>();
}

FPCGExContext::~FPCGExContext()
{
	WorkHandle.Reset();
	ManagedObjects->Flush(); // So cleanups can be recursively triggered while manager is still alive
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
			// Force execution on main thread
			FSharedContext<FPCGExContext> SharedContext(GetOrCreateHandle());
			if (!SharedContext.Get()) { return; }

			FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady(
				[SharedContext, FunctionNames]()
				{
					const FPCGExContext* Ctx = SharedContext.Get();
					for (TArray<AActor*> NotifyActorsArray = Ctx->NotifyActors.Array();
					     AActor* TargetActor : NotifyActorsArray)
					{
						if (!IsValid(TargetActor)) { continue; }
						for (UFunction* Function : PCGExHelpers::FindUserFunctions(TargetActor->GetClass(), FunctionNames, {UPCGExFunctionPrototypes::GetPrototypeWithNoParams()}, Ctx))
						{
							TargetActor->ProcessEvent(Function, nullptr);
						}
					}
				}, TStatId(), nullptr, ENamedThreads::GameThread);

			FTaskGraphInterface::Get().WaitUntilTaskCompletes(Task);
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


void FPCGExContext::SetAsyncState(const PCGExCommon::ContextState WaitState)
{
	// TODO : SetAsyncState is moot now
	SetState(WaitState);
}

bool FPCGExContext::IsWaitingForTasks()
{
	if (AsyncManager) { return AsyncManager->IsWaitingForTasks(); }
	return false;
}

void FPCGExContext::ReadyForExecution()
{
	bIsPaused = false;
	SetState(PCGExCommon::State_InitialExecution);
}

void FPCGExContext::SetState(const PCGExCommon::ContextState StateId)
{
	CurrentState.store(StateId, std::memory_order_release);
}

void FPCGExContext::Done()
{
	SetState(PCGExCommon::State_Done);
}

bool FPCGExContext::TryComplete(const bool bForce)
{
	if (IsWorkCompleted()) { return true; }
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
	// Ensure only one thread processes at a time
	bool bExpected = false;
	if (!bAsyncWorkEnded.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
	{
		// Another thread is already processing
		UE_LOG(LogTemp, Warning, TEXT("OnAsyncWorkEnd: Already processing, skipping (%s)"), *GetNameSafe(GetInputSettings<UPCGExSettings>()));
		return;
	}

	// RAII-style cleanup to ensure flag is reset even if exception occurs
	struct FProcessingGuard
	{
		std::atomic<bool>& Flag;
		~FProcessingGuard() { Flag.store(false, std::memory_order_release); }
	} Guard{bAsyncWorkEnded};

	if (bWasCancelled)
	{
		return;
	}

	// Reset manager BEFORE calling AdvanceWork
	// This is safe because we're the only thread that can be here
	if (AsyncManager) 
	{ 
		AsyncManager->Reset(); 
	}

	const UPCGExSettings* Settings = GetInputSettings<UPCGExSettings>();

	switch (CurrentPhase)
	{
	case EPCGExecutionPhase::NotExecuted:
		break;
	case EPCGExecutionPhase::PrepareData:
		ElementHandle->AdvancePreparation(this, Settings);
		break;
	case EPCGExecutionPhase::Execute:
		ElementHandle->AdvanceWork(this, Settings);
		break;
	case EPCGExecutionPhase::PostExecute:
		break;
	case EPCGExecutionPhase::Done:
		break;
	}
    
	// Guard destructor automatically resets bAsyncWorkEnded
}

void FPCGExContext::OnComplete()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExContext::OnComplete);

	//UE_LOG(LogTemp, Warning, TEXT(">> OnComplete @%s"), *GetInputSettings<UPCGExSettings>()->GetName());

	if (ElementHandle) { ElementHandle->CompleteWork(this); }

	FWriteScopeLock WriteScopeLock(StagedOutputLock);
	ManagedObjects->Remove(OutputData.TaggedData);
	bIsPaused = false;
}

#pragma endregion

#pragma region Async resource management

void FPCGExContext::CancelAssetLoading()
{
	if (LoadHandle.IsValid() && LoadHandle->IsActive()) { LoadHandle->CancelHandle(); }
	LoadHandle.Reset();
	if (RequiredAssets) { RequiredAssets->Empty(); }
}

TSet<FSoftObjectPath>& FPCGExContext::GetRequiredAssets()
{
	FWriteScopeLock WriteScopeLock(AssetDependenciesLock);
	if (!RequiredAssets) { RequiredAssets = MakeShared<TSet<FSoftObjectPath>>(); }
	return *RequiredAssets.Get();
}

void FPCGExContext::RegisterAssetDependencies()
{
}

void FPCGExContext::AddAssetDependency(const FSoftObjectPath& Dependency)
{
	FWriteScopeLock WriteScopeLock(AssetDependenciesLock);
	if (!RequiredAssets) { RequiredAssets = MakeShared<TSet<FSoftObjectPath>>(); }
	RequiredAssets->Add(Dependency);
}

void FPCGExContext::LoadAssets()
{
	if (bAssetLoadRequested) { return; }
	bAssetLoadRequested = true;

	SetState(PCGExCommon::State_LoadingAssetDependencies);

	if (!RequiredAssets || RequiredAssets->IsEmpty())
	{
		bAssetLoadError = true; // No asset to load, yet we required it?
		return;
	}

	if (!bForceSynchronousAssetLoad)
	{
		PauseContext();

		// Dispatch the async load request to the game thread
		TWeakPtr<FPCGContextHandle> CtxHandle = GetOrCreateHandle();

		if (IsInGameThread())
		{
			LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
				RequiredAssets->Array(), [CtxHandle]()
				{
					PCGEX_SHARED_CONTEXT_VOID(CtxHandle)
					SharedContext.Get()->OnAsyncWorkEnd(false);
				});

			if (!LoadHandle || !LoadHandle->IsActive())
			{
				if (!LoadHandle || !LoadHandle->HasLoadCompleted())
				{
					bAssetLoadError = true;
					CancelExecution("Error loading assets.");
				}
				else
				{
					OnAsyncWorkEnd(false);
				}
			}
		}
		else
		{
			AsyncTask(
				ENamedThreads::GameThread, [CtxHandle]()
				{
					PCGEX_SHARED_CONTEXT_VOID(CtxHandle)

					FPCGExContext* This = SharedContext.Get();

					This->LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
						This->RequiredAssets->Array(), [CtxHandle]()
						{
							PCGEX_SHARED_CONTEXT_VOID(CtxHandle)
							SharedContext.Get()->OnAsyncWorkEnd(false);
						});

					if (!This->LoadHandle || !This->LoadHandle->IsActive())
					{
						if (!This->LoadHandle || !This->LoadHandle->HasLoadCompleted())
						{
							This->bAssetLoadError = true;
							This->CancelExecution("Error loading assets.");
						}
						else
						{
							This->OnAsyncWorkEnd(false);
						}
					}
				});
		}
	}
	else
	{
		LoadHandle = UAssetManager::GetStreamableManager().RequestSyncLoad(RequiredAssets->Array());
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

	if (IPCGExManagedComponentInterface* Managed = Cast<IPCGExManagedComponentInterface>(InComponent)) { Managed->SetManagedComponent(ManagedComponent); }

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
		if (!InReason.IsEmpty() && !bQuietCancellationError) { PCGE_LOG_C(Error, GraphAndLog, this, FTEXT(InReason)); }

		PCGEX_TERMINATE_ASYNC

		OutputData.Reset();
		if (bPropagateAbortedExecution) { OutputData.bCancelExecution = true; }

		CancelAssetLoading();
		bIsPaused = false;
	}

	return true;
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
