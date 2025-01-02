// Copyright 2024 Timothé Lapetite and contributors
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

void FPCGExContext::StageOutput(const FName Pin, UPCGData* InData, const TSet<FString>& InTags, const bool bManaged, const bool bIsMutable)
{
	if (!IsInGameThread())
	{
		FWriteScopeLock WriteScopeLock(StagedOutputLock);

		AdditionsSinceLastReserve++;
		FPCGTaggedData& Output = StagedOutputs.Emplace_GetRef();
		Output.Pin = Pin;
		Output.Data = InData;
		Output.Tags.Append(InTags);
	}
	else
	{
		AdditionsSinceLastReserve++;
		FPCGTaggedData& Output = StagedOutputs.Emplace_GetRef();
		Output.Pin = Pin;
		Output.Data = InData;
		Output.Tags.Append(InTags);
	}

	if (bManaged) { ManagedObjects->Add(InData); }
	if (bIsMutable && bCleanupConsumableAttributes)
	{
#if PCGEX_ENGINE_VERSION > 503
		if (UPCGMetadata* Metadata = InData->MutableMetadata())
		{
			for (const FName ConsumableName : ConsumableAttributesSet)
			{
				if (!Metadata->HasAttribute(ConsumableName) || ProtectedAttributesSet.Contains(ConsumableName)) { continue; }
				Metadata->DeleteAttribute(ConsumableName);
			}
		}
#else
		if(const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(InData); SpatialData->Metadata)
		{
			for (const FName ConsumableName : ConsumableAttributesSet)
			{
				if (!SpatialData->Metadata->HasAttribute(ConsumableName) || !ProtectedAttributesSet.Contains(ConsumableName)) { continue; }
				SpatialData->Metadata->DeleteAttribute(ConsumableName);
			}
		}
#endif
	}
}

void FPCGExContext::StageOutput(const FName Pin, UPCGData* InData, const bool bManaged)
{
	if (!IsInGameThread())
	{
		FWriteScopeLock WriteScopeLock(StagedOutputLock);

		AdditionsSinceLastReserve++;
		FPCGTaggedData& Output = StagedOutputs.Emplace_GetRef();
		Output.Pin = Pin;
		Output.Data = InData;
	}
	else
	{
		AdditionsSinceLastReserve++;
		FPCGTaggedData& Output = StagedOutputs.Emplace_GetRef();
		Output.Pin = Pin;
		Output.Data = InData;
	}

	if (bManaged) { ManagedObjects->Add(InData); }
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
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExContext::WriteFutureOutputs);

	OutputData.TaggedData.Reserve(OutputData.TaggedData.Num() + StagedOutputs.Num());
	for (const FPCGTaggedData& FData : StagedOutputs)
	{
		ManagedObjects->Remove(const_cast<UPCGData*>(FData.Data.Get()));
		OutputData.TaggedData.Add(FData);
	}

	StagedOutputs.Empty();
}

FPCGExContext::FPCGExContext()
{
	Lifecycle = MakeShared<PCGEx::FLifecycle>();
	ManagedObjects = MakeUnique<PCGEx::FManagedObjects>(this, Lifecycle);
}

FPCGExContext::~FPCGExContext()
{
	Lifecycle->Terminate();

	CancelAssetLoading();

	ManagedObjects->Flush(); // So cleanups can be recursively triggered while manager is still alive
}

void FPCGExContext::StagedOutputReserve(const int32 NumAdditions)
{
	const int32 ConservativeAdditions = NumAdditions - FMath::Min(0, LastReserve - AdditionsSinceLastReserve);
	FWriteScopeLock WriteScopeLock(StagedOutputLock);
	LastReserve = ConservativeAdditions;
	StagedOutputs.Reserve(StagedOutputs.Num() + ConservativeAdditions);
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
	if (!bAsyncEnabled)
	{
		SetState(WaitState);
		return;
	}

	bWaitingForAsyncCompletion = true;
	SetState(WaitState);
}

bool FPCGExContext::ShouldWaitForAsync()
{
	if (!bAsyncEnabled)
	{
		if (bWaitingForAsyncCompletion) { ResumeExecution(); }
		return false;
	}

	return bWaitingForAsyncCompletion;
}

void FPCGExContext::ReadyForExecution()
{
	SetState(PCGEx::State_InitialExecution);
}

void FPCGExContext::SetState(const PCGEx::ContextState StateId)
{
	if (CurrentState == StateId) { return; }
	CurrentState = StateId;
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
					if (FPCGExContext* NestedThis = GetContextFromHandle<FPCGExContext>(CtxHandle)) { NestedThis->UnpauseContext(); }
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
					FPCGExContext* This = GetContextFromHandle<FPCGExContext>(CtxHandle);
					if (!This) { return; }

					This->LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
						This->RequiredAssets->Array(), [CtxHandle]()
						{
							if (FPCGExContext* NestedThis = GetContextFromHandle<FPCGExContext>(CtxHandle)) { NestedThis->UnpauseContext(); }
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
	UPCGComponent* SrcComp = SourceComponent.Get();

	bool bIsPreviewMode = false;
#if PCGEX_ENGINE_VERSION > 503
	bIsPreviewMode = SrcComp->IsInPreviewMode();
#endif

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

	if (InComponent->Implements<UPCGExManagedComponentInterface>())
	{
		if (IPCGExManagedComponentInterface* Managed = Cast<IPCGExManagedComponentInterface>(InComponent)) { Managed->SetManagedComponent(ManagedComponent); }
	}

	InParent->Modify(!bIsPreviewMode);

	InComponent->RegisterComponent();
	InParent->AddInstanceComponent(InComponent);

	if (USceneComponent* SceneComponent = Cast<USceneComponent>(InComponent))
	{
		SceneComponent->AttachToComponent(InParent->GetRootComponent(), AttachmentRules);
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
#if WITH_EDITOR && PCGEX_ENGINE_VERSION > 503
	if (UPCGComponent* PCGComponent = SourceComponent.Get())
	{
		TPair<FPCGSelectionKey, bool> NewPair(FPCGSelectionKey::CreateFromPath(Path), bIsCulled);
		PCGComponent->RegisterDynamicTracking(GetOriginalSettings<UPCGSettings>(), MakeArrayView(&NewPair, 1));
	}
#endif
}

void FPCGExContext::EDITOR_TrackClass(const TSubclassOf<UObject>& InSelectionClass, bool bIsCulled) const
{
#if WITH_EDITOR && PCGEX_ENGINE_VERSION > 503
	if (UPCGComponent* PCGComponent = SourceComponent.Get())
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
	bExecutionCancelled = true;
	Lifecycle->Terminate();
	ResumeExecution();
	if (!InReason.IsEmpty()) { PCGE_LOG_C(Error, GraphAndLog, this, FTEXT(InReason)); }
	return true;
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
