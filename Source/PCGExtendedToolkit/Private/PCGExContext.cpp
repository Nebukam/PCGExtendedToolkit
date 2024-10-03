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

	if (bManaged) { ManagedObjects->Add(InData);}
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
		ManagedObjects->Remove(const_cast<UPCGData*>(FData.Data.Get()));
		OutputData.TaggedData.Add(FData);
	}

	StagedOutputs.Empty();
}

FPCGExContext::FPCGExContext()
{
	ManagedObjects = MakeShared<PCGEx::FManagedObjects>(this);
}

FPCGExContext::~FPCGExContext()
{
	CancelAssetLoading();
	
	ManagedObjects->Flush();
	ManagedObjects.Reset();
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
