// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "PCGExBlueprintHelpers.generated.h"

UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExBlueprintHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//UFUNCTION(BlueprintCallable, Category = "PCG", meta=(DisplayName = "Flush PCG Cache"))
	//static bool FlushPCGCache();
};
