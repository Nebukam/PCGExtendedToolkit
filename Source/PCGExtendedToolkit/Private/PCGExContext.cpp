// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExContext.h"

#define LOCTEXT_NAMESPACE "PCGExContext"

FPCGTaggedData& FPCGExContext::NewOutput()
{
	FWriteScopeLock WriteScopeLock(ContextOutputLock);
	return OutputData.TaggedData.Emplace_GetRef();
}

FPCGTaggedData& FPCGExContext::NewOutput(const FName Pin, UPCGData* InData)
{
	FPCGTaggedData& Output = NewOutput();
	Output.Pin = Pin;
	Output.Data = InData;
	return Output;
}


#undef LOCTEXT_NAMESPACE
