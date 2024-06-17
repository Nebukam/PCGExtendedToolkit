// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExContext.h"

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

void FPCGExContext::ExecuteEnd()
{
	WriteFutureOutputs();
}


#undef LOCTEXT_NAMESPACE
