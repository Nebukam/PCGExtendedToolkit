// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGPoint.h"
#include "PCGExOperation.h"


#include "Paths/PCGExPaths.h"
#include "PCGExSubPointsOperation.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSubPointsOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	bool bClosedLoop = false;

	bool bPreserveTransform = false;
	bool bPreservePosition = false;
	bool bPreserveRotation = false;
	bool bPreserveScale = false;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual void PrepareForData(const TSharedPtr<PCGExData::FFacade>& InPrimaryFacade, const TSet<FName>* IgnoreAttributeSet);

	virtual void ProcessSubPoints(
		const PCGExData::FPointRef& From,
		const PCGExData::FPointRef& To,
		const TArrayView<FPCGPoint>& SubPoints,
		const PCGExPaths::FPathMetrics& Metrics,
		const int32 StartIndex = -1) const;
};
