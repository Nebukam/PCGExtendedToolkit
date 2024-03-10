// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGPoint.h"
#include "Data/PCGExAttributeHelpers.h"
#include "PCGExOperation.h"
#include "PCGExSubPointsOperation.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	bool bClosedPath = false;

	virtual void PrepareForData(PCGExData::FPointIO& InPointIO);
	virtual void ProcessPoints(UPCGPointData* InData) const;
	virtual void ProcessSubPoints(const PCGEx::FPointRef& Start, const PCGEx::FPointRef& End, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetricsSquared& Metrics) const;
	virtual void ProcessSubPoints(TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetricsSquared& Metrics, const int32 Offset) const;
};
