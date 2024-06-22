// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Paths/SubPoints/PCGExSubPointsOperation.h"
#include "PCGExOrientOperation.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExOrientOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	bool bClosedPath = false;
	EPCGExAxis OrientAxis = EPCGExAxis::Forward;
	EPCGExAxis UpAxis = EPCGExAxis::Up;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual void PrepareForData(PCGExData::FPointIO* InPointIO);
	virtual FTransform ComputeOrientation(const PCGEx::FPointRef& Point, const PCGEx::FPointRef& Previous, const PCGEx::FPointRef& Next, const double DirectionMultiplier) const;
};
