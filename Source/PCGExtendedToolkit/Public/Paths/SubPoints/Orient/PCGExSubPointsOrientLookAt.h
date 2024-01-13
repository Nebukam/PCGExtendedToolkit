// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsOrientOperation.h"
#include "PCGExSubPointsOrientLookAt.generated.h"

UENUM(BlueprintType)
enum class EPCGExOrientLookAt : uint8
{
	NextPoint UMETA(DisplayName = "Next Point", ToolTip="Look at next point in path"),
	PreviousPoint UMETA(DisplayName = "Previous Point", ToolTip="Look at previous point in path"),
	Attribute UMETA(DisplayName = "Attribute", ToolTip="Look at a local vector attribute representing a world position"),
};

/**
 * 
 */
UCLASS(DisplayName = "Look At")
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsOrientLookAt : public UPCGExSubPointsOrientOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOrientLookAt LookAt = EPCGExOrientLookAt::NextPoint;

	virtual void ProcessSubPoints(const PCGEx::FPointRef& Start, const PCGEx::FPointRef& End, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetrics& Metrics) const override;

	virtual void LookAtNext(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const;
	virtual void LookAtPrev(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const;
	virtual void LookAtAttribute(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const;
};
