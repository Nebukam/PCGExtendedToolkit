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
	
public:
	FPCGTaggedData& NewOutput();
	FPCGTaggedData& NewOutput(const FName Pin, UPCGData* InData);
	
};
