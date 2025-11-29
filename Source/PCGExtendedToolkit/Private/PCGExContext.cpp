// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExContext.h"

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
	CancelAssetLoading();
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
	bWaitingForAsyncCompletion = true;
	SetState(WaitState);
}

bool FPCGExContext::ShouldWaitForAsync()
{
	if (!AsyncManager)
	{
		if (bWaitingForAsyncCompletion) { ResumeExecution(); }
		return false;
	}

	return bWaitingForAsyncCompletion;
}

void FPCGExContext::ReadyForExecution()
{
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

void FPCGExContext::ResumeExecution()
{
	if (AsyncManager) { AsyncManager->Reset(); }
	UnpauseContext();
	bWaitingForAsyncCompletion = false;
}

void FPCGExContext::OnComplete()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExContext::OnComplete);

	FWriteScopeLock WriteScopeLock(StagedOutputLock);
	ManagedObjects->Remove(OutputData.TaggedData);
}

#pragma endregion

#pragma region Async resource management

void FPCGExContext::CancelAssetLoading()
{
	if (LoadHandle.IsValid() && LoadHandle->IsActive()) { LoadHandle->CancelHandle(); }
	LoadHandle.Reset();
	if (RequiredAssets) { RequiredAssets->Empty(); }
	CancelExecution(TEXT("")); // Quiet cancel
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

	SetAsyncState(PCGExCommon::State_LoadingAssetDependencies);

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
					SharedContext.Get()->UnpauseContext();
				});

			if (!LoadHandle || !LoadHandle->IsActive())
			{
				UnpauseContext();

				if (!LoadHandle || !LoadHandle->HasLoadCompleted())
				{
					bAssetLoadError = true;
					CancelExecution("Error loading assets.");
				}
				else
				{
					// Resources were already loaded
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
							SharedContext.Get()->UnpauseContext();
						});

					if (!This->LoadHandle || !This->LoadHandle->IsActive())
					{
						This->UnpauseContext();

						if (!This->LoadHandle || !This->LoadHandle->HasLoadCompleted())
						{
							This->bAssetLoadError = true;
							This->CancelExecution("Error loading assets.");
						}
						else
						{
							// Resources were already loaded
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

bool FPCGExContext::IsAsyncWorkComplete()
{
	// Context must be unpaused for this to be called
	if (!bWaitingForAsyncCompletion || !AsyncManager) { return true; }

	// TODO : We want the async manager to notify that work can resume
	// not the other way around
	// so we need a pointer to the IPCGElement :x

	if (!AsyncManager->IsWaitingForRunningTasks())
	{
		ResumeExecution();
		return true;
	}

	return false;
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

		ResumeExecution();
	}

	return true;
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
