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
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSubPointsOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	bool bClosedPath = false;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual void PrepareForData(PCGExData::FFacade* InPrimaryFacade);
	virtual void ProcessPoints(UPCGPointData* InData) const;
	virtual void ProcessSubPoints(const PCGExData::FPointRef& Start, const PCGExData::FPointRef& End, const TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetricsSquared& Metrics) const;
	virtual void ProcessSubPoints(const TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetricsSquared& Metrics, const int32 Offset) const;
};
