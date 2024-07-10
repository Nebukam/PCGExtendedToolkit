// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExContext.h"

#include "Data/PCGSpatialData.h"

#define LOCTEXT_NAMESPACE "PCGExContext"

void FPCGExContext::FutureOutput(const FName Pin, UPCGData* InData, const TSet<FString>& InTags)
{
	FWriteScopeLock WriteScopeLock(ContextOutputLock);
	FPCGTaggedData& Output = FutureOutputs.Emplace_GetRef();
	Output.Pin = Pin;
	Output.Data = InData;
	Output.Tags.Append(InTags);
}

void FPCGExContext::FutureOutput(const FName Pin, UPCGData* InData)
{
	FWriteScopeLock WriteScopeLock(ContextOutputLock);
	FPCGTaggedData& Output = FutureOutputs.Emplace_GetRef();
	Output.Pin = Pin;
	Output.Data = InData;
}

void FPCGExContext::WriteFutureOutputs()
{
	for (FPCGTaggedData FutureData : FutureOutputs) { OutputData.TaggedData.Add(FutureData); }
	FutureOutputs.Empty();
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
