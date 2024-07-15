// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExContext.h"

#include "PCGExMacros.h"
#include "Data/PCGSpatialData.h"

#define LOCTEXT_NAMESPACE "PCGExContext"

void FPCGExContext::FutureOutput(const FName Pin, UPCGData* InData, const TSet<FString>& InTags)
{
	if (bUseLock) { ContextOutputLock.WriteLock(); }

	AdditionsSinceLastReserve++;

	FPCGTaggedData& Output = FutureOutputs.Emplace_GetRef();
	Output.Pin = Pin;
	Output.Data = InData;
	Output.Tags.Append(InTags);

	if (bUseLock) { ContextOutputLock.WriteUnlock(); }
}

void FPCGExContext::FutureOutput(const FName Pin, UPCGData* InData)
{
	if (bUseLock) { ContextOutputLock.WriteLock(); }

	AdditionsSinceLastReserve++;

	FPCGTaggedData& Output = FutureOutputs.Emplace_GetRef();
	Output.Pin = Pin;
	Output.Data = InData;

	if (bUseLock) { ContextOutputLock.WriteUnlock(); }
}

void FPCGExContext::WriteFutureOutputs()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExContext::WriteFutureOutputs);
	
	OutputData.TaggedData.Reserve(OutputData.TaggedData.Num() + FutureOutputs.Num());
	for (const FPCGTaggedData& FData : FutureOutputs) { OutputData.TaggedData.Add(FData); }

	FutureOutputs.Empty();
}

FPCGExContext::~FPCGExContext()
{
	FutureOutputs.Empty();
}

void FPCGExContext::FutureReserve(const int32 NumAdditions)
{
	if (bUseLock) { ContextOutputLock.WriteLock(); }

	const int32 ConservativeAdditions = NumAdditions - FMath::Min(0, LastReserve - AdditionsSinceLastReserve);
	LastReserve = ConservativeAdditions;
	FutureOutputs.Reserve(FutureOutputs.Num() + ConservativeAdditions);

	if (bUseLock) { ContextOutputLock.WriteUnlock(); }
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


#undef LOCTEXT_NAMESPACE
