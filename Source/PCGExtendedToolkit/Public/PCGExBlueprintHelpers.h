// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGExHelpers.h"
#include "Data/PCGExGridTracking.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PCGExBlueprintHelpers.generated.h"

UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExBlueprintHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Generates a grid ID from a component and a location. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PCGEx", meta=(CallableWithoutWorldContext))
	static FPCGExGridID MakeGridIDFromComponentAndLocation(const UPCGComponent* InPCGComponent, const FVector& InLocation, const FName InName = NAME_None)
	{
		return FPCGExGridID(InPCGComponent, InLocation, InName);
	}

	/** Generates a grid ID from a component. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PCGEx", meta=(CallableWithoutWorldContext), BlueprintPure)
	static FPCGExGridID MakeGridIDFromComponent(const UPCGComponent* InPCGComponent, const FName InName = NAME_None)
	{
		return FPCGExGridID(InPCGComponent, InName);
	}

	/** Gets the uint32 hash generated from a FPCGExGridID */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PCGEx", meta=(CallableWithoutWorldContext))
	static int32 GetGridIDHash(const FPCGExGridID& InGridID)
	{
		return InGridID;
	}
};
