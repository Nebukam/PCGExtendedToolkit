// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExContext.h"

#include "PCGExMacros.h"
#include "Data/PCGSpatialData.h"
#include "Engine/AssetManager.h"

#define LOCTEXT_NAMESPACE "PCGExContext"

void FPCGExContext::StageOutput(const FName Pin, UPCGData* InData, const TSet<FString>& InTags, const bool bManaged)
{
	if (!IsInGameThread())
	{
		FWriteScopeLock WriteScopeLock(ContextOutputLock);

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

	if (bManaged) { AddManagedObject(InData); }
}

void FPCGExContext::StageOutput(const FName Pin, UPCGData* InData, const bool bManaged)
{
	if (!IsInGameThread())
	{
		FWriteScopeLock WriteScopeLock(ContextOutputLock);

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

	if (bManaged) { AddManagedObject(InData); }
}

void FPCGExContext::AddManagedObject(UObject* InObject)
{
	FWriteScopeLock WriteScopeLock(ManagedObjectLock);
	ManagedObjects.Add(InObject);
	InObject->AddToRoot();
}

void FPCGExContext::ReleaseManagedObject(UObject* InObject)
{
	if (!IsValid(InObject)) { return; }
	
	{
		FWriteScopeLock WriteScopeLock(ManagedObjectLock);
		ManagedObjects.Remove(InObject);
		if (InObject->IsRooted()) { InObject->RemoveFromRoot(); }

		if (InObject->HasAnyInternalFlags(EInternalObjectFlags::Async))
		{
			InObject->ClearInternalFlags(EInternalObjectFlags::Async);

			ForEachObjectWithOuter(
				InObject, [&](UObject* SubObject)
				{
					if (ManagedObjects.Contains(SubObject)) { return; }
					SubObject->ClearInternalFlags(EInternalObjectFlags::Async);
				}, true);
		}
	}
}

void FPCGExContext::StartAsyncWork()
{
	bIsPaused = true;
}

void FPCGExContext::StopAsyncWork()
{
	bIsPaused = false;
}

void FPCGExContext::CommitStagedOutputs()
{
	// Must be executed on main thread
	ensure(IsInGameThread());

	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExContext::WriteFutureOutputs);

	OutputData.TaggedData.Reserve(OutputData.TaggedData.Num() + StagedOutputs.Num());
	for (const FPCGTaggedData& FData : StagedOutputs)
	{
		ReleaseManagedObject(const_cast<UPCGData*>(FData.Data.Get()));
		OutputData.TaggedData.Add(FData);
	}

	StagedOutputs.Empty();
}

FPCGExContext::~FPCGExContext()
{
	CancelAssetLoading();

	// Flush remaining managed objects & mark them as garbage
	for (UObject* ObjectPtr : ManagedObjects)
	{
		if (ObjectPtr->IsRooted()) { ObjectPtr->RemoveFromRoot(); }

		if (ObjectPtr->HasAnyInternalFlags(EInternalObjectFlags::Async))
		{
			ObjectPtr->ClearInternalFlags(EInternalObjectFlags::Async);

			ForEachObjectWithOuter(
				ObjectPtr, [&](UObject* SubObject)
				{
					if (ManagedObjects.Contains(SubObject)) { return; }
					SubObject->ClearInternalFlags(EInternalObjectFlags::Async);
				}, true);
		}

		ObjectPtr->MarkAsGarbage();
	}

	ManagedObjects.Empty();
}

void FPCGExContext::StagedOutputReserve(const int32 NumAdditions)
{
	const int32 ConservativeAdditions = NumAdditions - FMath::Min(0, LastReserve - AdditionsSinceLastReserve);

	if (bUseLock)
	{
		FWriteScopeLock WriteScopeLock(ContextOutputLock);
		LastReserve = ConservativeAdditions;
		StagedOutputs.Reserve(StagedOutputs.Num() + ConservativeAdditions);
		return;
	}

	LastReserve = ConservativeAdditions;
	StagedOutputs.Reserve(StagedOutputs.Num() + ConservativeAdditions);
}

void FPCGExContext::DeleteManagedObject(UObject* InObject)
{
	check(InObject)

	if (!IsValid(InObject)) { return; }

	ReleaseManagedObject(InObject);
	InObject->MarkAsGarbage();
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

#pragma region Async resource management

void FPCGExContext::CancelAssetLoading()
{
	if (LoadHandle.IsValid() && LoadHandle->IsActive()) { LoadHandle->CancelHandle(); }
	LoadHandle.Reset();
	RequiredAssets.Empty();
}

void FPCGExContext::RegisterAssetDependencies()
{
}

void FPCGExContext::RegisterAssetRequirement(const FSoftObjectPath& Dependency)
{
	RequiredAssets.Add(Dependency);
}

void FPCGExContext::LoadAssets()
{
	if (WasAssetLoadRequested()) { return; }

	bAssetLoadRequested = true;

	if (RequiredAssets.IsEmpty())
	{
		bAssetLoadError = true; // No asset to load, yet we required it?
		return;
	}

	if (!bForceSynchronousAssetLoad)
	{
		StartAsyncWork();

		LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
			RequiredAssets.Array(), [&]()
			{
				StopAsyncWork();
			});

		if (!LoadHandle || !LoadHandle->IsActive())
		{
			// Huh
			bAssetLoadError = true;
			StopAsyncWork();
		}
	}
	else
	{
		LoadHandle = UAssetManager::GetStreamableManager().RequestSyncLoad(RequiredAssets.Array());
	}
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
