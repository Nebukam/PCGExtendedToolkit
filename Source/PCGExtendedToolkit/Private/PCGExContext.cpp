// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExContext.h"

#include "PCGExMacros.h"
#include "Data/PCGSpatialData.h"
#include "Engine/AssetManager.h"

#define LOCTEXT_NAMESPACE "PCGExContext"

void FPCGExContext::FutureOutput(const FName Pin, UPCGData* InData, const TSet<FString>& InTags)
{
	if (bUseLock)
	{
		FWriteScopeLock WriteScopeLock(ContextOutputLock);
		AdditionsSinceLastReserve++;
		FPCGTaggedData& Output = FutureOutputs.Emplace_GetRef();
		Output.Pin = Pin;
		Output.Data = InData;
		Output.Tags.Append(InTags);
	}
	else
	{
		AdditionsSinceLastReserve++;
		FPCGTaggedData& Output = FutureOutputs.Emplace_GetRef();
		Output.Pin = Pin;
		Output.Data = InData;
		Output.Tags.Append(InTags);
	}
}

void FPCGExContext::FutureOutput(const FName Pin, UPCGData* InData)
{
	if (bUseLock)
	{
		FWriteScopeLock WriteScopeLock(ContextOutputLock);
		AdditionsSinceLastReserve++;
		FPCGTaggedData& Output = FutureOutputs.Emplace_GetRef();
		Output.Pin = Pin;
		Output.Data = InData;
	}
	else
	{
		AdditionsSinceLastReserve++;
		FPCGTaggedData& Output = FutureOutputs.Emplace_GetRef();
		Output.Pin = Pin;
		Output.Data = InData;
	}
}

void FPCGExContext::WriteFutureOutputs()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExContext::WriteFutureOutputs);

	UnrootFutures();

	OutputData.TaggedData.Reserve(OutputData.TaggedData.Num() + FutureOutputs.Num());
	for (const FPCGTaggedData& FData : FutureOutputs) { OutputData.TaggedData.Add(FData); }

	FutureOutputs.Empty();
}

FPCGExContext::~FPCGExContext()
{
	CancelLoading();
	UnrootFutures();
	FutureOutputs.Empty();
}

void FPCGExContext::UnrootFutures()
{
	for (UPCGData* FData : Rooted) { PCGEX_UNROOT(FData) }
	Rooted.Empty();
}

void FPCGExContext::FutureReserve(const int32 NumAdditions)
{
	const int32 ConservativeAdditions = NumAdditions - FMath::Min(0, LastReserve - AdditionsSinceLastReserve);

	if (bUseLock)
	{
		FWriteScopeLock WriteScopeLock(ContextOutputLock);
		LastReserve = ConservativeAdditions;
		FutureOutputs.Reserve(FutureOutputs.Num() + ConservativeAdditions);
		Rooted.Reserve(Rooted.Num() + ConservativeAdditions);
		return;
	}

	LastReserve = ConservativeAdditions;
	FutureOutputs.Reserve(FutureOutputs.Num() + ConservativeAdditions);
}

void FPCGExContext::FutureRootedOutput(const FName Pin, UPCGData* InData, const TSet<FString>& InTags)
{
	// Keep our data rooted until we are destroyed, or until they are output'd
	if (bUseLock)
	{
		FWriteScopeLock WriteScopeLock(ContextOutputLock);
		Rooted.Add(InData);
	}
	else { Rooted.Add(InData); }
	FutureOutput(Pin, InData, InTags);
}

void FPCGExContext::OnComplete()
{
	WriteFutureOutputs();

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

bool FPCGExContext::RequestResourceLoad(FPCGContext* ThisContext, TArray<FSoftObjectPath>&& ObjectsToLoad, bool bAsynchronous)
{
	if (!ObjectsToLoad.IsEmpty() && !bLoadRequested)
	{
		if (!bAsynchronous)
		{
			LoadHandle = UAssetManager::GetStreamableManager().RequestSyncLoad(std::forward<TArray<FSoftObjectPath>>(ObjectsToLoad));
			bLoadRequested = true;

			return true;
		}
		else
		{
			ThisContext->bIsPaused = true;

			// It is a bit unsafe to pass this to a delegate lambda. But if the context dies before the completion of the loading, the context will cancel the loading in its dtor.
			LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(std::forward<TArray<FSoftObjectPath>>(ObjectsToLoad), [ThisContext]() { ThisContext->bIsPaused = false; });
			bLoadRequested = true;

			// If the load handle is not active it means objects were invalid
			if (!LoadHandle->IsActive())
			{
				ThisContext->bIsPaused = false;
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	return true;
}

void FPCGExContext::CancelLoading()
{
	if (LoadHandle.IsValid() && LoadHandle->IsActive()) { LoadHandle->CancelHandle(); }
	LoadHandle.Reset();
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
