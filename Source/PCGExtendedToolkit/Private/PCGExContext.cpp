// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExContext.h"

#include "PCGComponent.h"
#include "PCGExMacros.h"
#include "PCGManagedResource.h"
#include "Data/PCGSpatialData.h"
#include "Engine/AssetManager.h"
#include "Helpers/PCGHelpers.h"
#include "Async/Async.h"

#define LOCTEXT_NAMESPACE "PCGExContext"

FPCGTaggedData& FPCGExContext::StageOutput(UPCGData* InData, const bool bManaged, const bool bIsMutable)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExContext::StageOutput);

	int32 Index = -1;
	if (!IsInGameThread())
	{
		FWriteScopeLock WriteScopeLock(StagedOutputLock);

		FPCGTaggedData& Output = StagedOutputs.Emplace_GetRef();
		Output.Data = InData;
		Index = StagedOutputs.Num()-1;
	}
	else
	{
		FPCGTaggedData& Output = StagedOutputs.Emplace_GetRef();
		Output.Data = InData;
		Index = StagedOutputs.Num()-1;
	}

	if (bManaged) { ManagedObjects->Add(InData); }
	if (bIsMutable && bCleanupConsumableAttributes)
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

	return StagedOutputs[Index];
}

FPCGTaggedData& FPCGExContext::StageOutput(UPCGData* InData, const bool bManaged)
{
	int32 Index = -1;
	if (!IsInGameThread())
	{
		FWriteScopeLock WriteScopeLock(StagedOutputLock);
		FPCGTaggedData& Output = StagedOutputs.Emplace_GetRef();
		Output.Data = InData;
		Index = StagedOutputs.Num()-1;
	}
	else
	{
		FPCGTaggedData& Output = StagedOutputs.Emplace_GetRef();
		Output.Data = InData;
		Index = StagedOutputs.Num()-1;
	}

	if (bManaged) { ManagedObjects->Add(InData); }
	return StagedOutputs[Index];
}

UWorld* FPCGExContext::GetWorld() const { return GetComponent()->GetWorld(); }

const UPCGComponent* FPCGExContext::GetComponent() const
{
	return Cast<UPCGComponent>(ExecutionSource.Get());
}

UPCGComponent* FPCGExContext::GetMutableComponent() const
{
	return const_cast<UPCGComponent*>(Cast<UPCGComponent>(ExecutionSource.Get()));
}

void FPCGExContext::PauseContext()
{
	bIsPaused = true;
}

void FPCGExContext::UnpauseContext()
{
	bIsPaused = false;
}

void FPCGExContext::CommitStagedOutputs()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExContext::CommitStagedOutputs);

	FWriteScopeLock WriteScopeLock(StagedOutputLock);
	
	ManagedObjects->Remove(StagedOutputs);

	OutputData.TaggedData.Reserve(OutputData.TaggedData.Num() + StagedOutputs.Num());
	OutputData.TaggedData.Append(StagedOutputs);

	StagedOutputs.Empty();
}

FPCGExContext::FPCGExContext()
{
	WorkPermit = MakeShared<PCGEx::FWorkPermit>();
	ManagedObjects = MakeShared<PCGEx::FManagedObjects>(this);
	UniqueNameGenerator = MakeShared<PCGEx::FUniqueNameGenerator>();
}

FPCGExContext::~FPCGExContext()
{
	WorkPermit.Reset();
	CancelAssetLoading();
	ManagedObjects->Flush(); // So cleanups can be recursively triggered while manager is still alive
}

void FPCGExContext::IncreaseStagedOutputReserve(const int32 InIncreaseNum)
{
	FWriteScopeLock WriteScopeLock(StagedOutputLock);
	StagedOutputs.Reserve(StagedOutputs.Max() + InIncreaseNum);
}

void FPCGExContext::ExecuteOnNotifyActors(const TArray<FName>& FunctionNames) const
{
	FReadScopeLock ReadScopeLock(NotifyActorsLock);

	// Execute PostProcess Functions
	if (!NotifyActors.IsEmpty())
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

void FPCGExContext::OnComplete()
{
	CommitStagedOutputs();

	if (bFlattenOutput)
	{
		TSet<uint64> InputUIDs;
		InputUIDs.Reserve(OutputData.TaggedData.Num());
		for (FPCGTaggedData& InTaggedData : InputData.TaggedData) { if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(InTaggedData.Data)) { InputUIDs.Add(SpatialData->UID); } }

		for (FPCGTaggedData& OutTaggedData : OutputData.TaggedData)
		{
			if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(OutTaggedData.Data); SpatialData && !InputUIDs.Contains(SpatialData->UID)) { SpatialData->Metadata->Flatten(); }
		}
	}
}


#pragma region State


void FPCGExContext::SetAsyncState(const PCGEx::ContextState WaitState)
{
	bWaitingForAsyncCompletion = true;
	SetState(WaitState);
}

bool FPCGExContext::ShouldWaitForAsync()
{
	return bWaitingForAsyncCompletion;
}

void FPCGExContext::ReadyForExecution()
{
	SetState(PCGEx::State_InitialExecution);
}

void FPCGExContext::SetState(const PCGEx::ContextState StateId)
{
	CurrentState.store(StateId, std::memory_order_release);
}

void FPCGExContext::Done()
{
	SetState(PCGEx::State_Done);
}

bool FPCGExContext::TryComplete(const bool bForce)
{
	if (!bForce && !IsDone()) { return false; }
	OnComplete();
	return true;
}

void FPCGExContext::ResumeExecution()
{
	UnpauseContext();
	bWaitingForAsyncCompletion = false;
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

	SetAsyncState(PCGEx::State_LoadingAssetDependencies);

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

void FPCGExContext::EDITOR_TrackPath(const FSoftObjectPath& Path, const bool bIsCulled) const
{
#if WITH_EDITOR
	if (UPCGComponent* PCGComponent = GetMutableComponent())
	{
		TPair<FPCGSelectionKey, bool> NewPair(FPCGSelectionKey::CreateFromPath(Path), bIsCulled);
		PCGComponent->RegisterDynamicTracking(GetOriginalSettings<UPCGSettings>(), MakeArrayView(&NewPair, 1));
	}
#endif
}

void FPCGExContext::EDITOR_TrackClass(const TSubclassOf<UObject>& InSelectionClass, bool bIsCulled) const
{
#if WITH_EDITOR
	if (UPCGComponent* PCGComponent = GetMutableComponent())
	{
		TPair<FPCGSelectionKey, bool> NewPair(FPCGSelectionKey(InSelectionClass), bIsCulled);
		PCGComponent->RegisterDynamicTracking(GetOriginalSettings<UPCGSettings>(), MakeArrayView(&NewPair, 1));
	}
#endif
}

bool FPCGExContext::CanExecute() const
{
	return !bExecutionCancelled;
}

bool FPCGExContext::CancelExecution(const FString& InReason)
{
	if (bExecutionCancelled) { return true; }

	bExecutionCancelled = true;
	WorkPermit.Reset();
	ResumeExecution();
	if (!InReason.IsEmpty()) { PCGE_LOG_C(Error, GraphAndLog, this, FTEXT(InReason)); }
	return true;
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
