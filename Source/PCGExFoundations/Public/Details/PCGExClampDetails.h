// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "CoreMinimal.h"
#include "PCGExClampDetails.generated.h"

USTRUCT(BlueprintType)
struct PCGEXFOUNDATIONS_API FPCGExClampDetails
{
	GENERATED_BODY()

	FPCGExClampDetails()
	{
	}

	FPCGExClampDetails(const FPCGExClampDetails& Other)
		: bApplyClampMin(Other.bApplyClampMin), ClampMinValue(Other.ClampMinValue), bApplyClampMax(Other.bApplyClampMax), ClampMaxValue(Other.ClampMaxValue)
	{
	}

	/** Clamp minimum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bApplyClampMin = false;

	/** Clamp minimum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bApplyClampMin"))
	double ClampMinValue = 0;

	/** Clamp maximum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bApplyClampMax = false;

	/** Clamp maximum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bApplyClampMax"))
	double ClampMaxValue = 0;

	FORCEINLINE double GetClampMin(const double InValue) const
	{
		return InValue < ClampMinValue ? ClampMinValue : InValue;
	}

	FORCEINLINE double GetClampMax(const double InValue) const
	{
		return InValue > ClampMaxValue ? ClampMaxValue : InValue;
	}

	FORCEINLINE double GetClampMinMax(const double InValue) const
	{
		return InValue > ClampMaxValue ? ClampMaxValue : InValue < ClampMinValue ? ClampMinValue : InValue;
	}

	FORCEINLINE double GetClampedValue(const double InValue) const
	{
		if (bApplyClampMin && InValue < ClampMinValue) { return ClampMinValue; }
		if (bApplyClampMax && InValue > ClampMaxValue) { return ClampMaxValue; }
		return InValue;
	}
};
