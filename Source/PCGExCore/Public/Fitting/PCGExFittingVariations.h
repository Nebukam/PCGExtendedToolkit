// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFittingCommon.h"
#include "PCGExFittingVariations.generated.h"

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExFittingVariations
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector OffsetMin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector OffsetMax = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExVariationSnapping SnapPosition = EPCGExVariationSnapping::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector OffsetSnap = FVector(100);

	/** Set offset in world space */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bAbsoluteOffset = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FRotator RotationMin = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FRotator RotationMax = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExVariationSnapping SnapRotation = EPCGExVariationSnapping::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FRotator RotationSnap = FRotator(90);

	/** Set rotation directly instead of additively on the selected axis */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExCore.EPCGExAbsoluteRotationFlags"))
	uint8 AbsoluteRotation = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (AllowPreserveRatio, PCG_Overridable))
	FVector ScaleMin = FVector::OneVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (AllowPreserveRatio, PCG_Overridable))
	FVector ScaleMax = FVector::OneVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExVariationSnapping SnapScale = EPCGExVariationSnapping::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector ScaleSnap = FVector(0.1);

	/** Scale uniformly on each axis. Uses the X component of ScaleMin and ScaleMax. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bUniformScale = true;

	void ApplyOffset(const FRandomStream& RandomStream, FTransform& OutTransform) const;
	void ApplyRotation(const FRandomStream& RandomStream, FTransform& OutTransform) const;
	void ApplyScale(const FRandomStream& RandomStream, FTransform& OutTransform) const;
};
