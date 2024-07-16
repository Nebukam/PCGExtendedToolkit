// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Runtime/Launch/Resources/Version.h"

#include "CoreMinimal.h"
#include "PCGContext.h"

struct PCGEXTENDEDTOOLKIT_API FPCGExContext : public FPCGContext
{
protected:
	mutable FRWLock ContextOutputLock;
	TArray<FPCGTaggedData> FutureOutputs;
	bool bFlattenOutput = false;
	bool bUseLock = true;

	int32 LastReserve = 0;
	int32 AdditionsSinceLastReserve = 0;

	void WriteFutureOutputs();

public:
	virtual ~FPCGExContext() override;

	void FutureReserve(const int32 NumAdditions);
	void FutureOutput(const FName Pin, UPCGData* InData, const TSet<FString>& InTags);
	void FutureOutput(const FName Pin, UPCGData* InData);

	virtual void OnComplete();
};
