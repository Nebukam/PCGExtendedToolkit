// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsOrientOperation.h"
#include "PCGExSubPointsOrientWeighted.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Weighted")
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsOrientWeighted : public UPCGExSubPointsOrientOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInverseWeight = false;

	virtual void ProcessSubPoints(const PCGEx::FPointRef& Start, const PCGEx::FPointRef& End, const TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetricsSquared& Metrics) const override;

	virtual void Orient(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const override;
	virtual void OrientInvertedWeight(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const;

protected:
	virtual void ApplyOverrides() override;
};
