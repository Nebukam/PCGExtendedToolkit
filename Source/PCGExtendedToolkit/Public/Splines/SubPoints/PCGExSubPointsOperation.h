// Copyright Timothé Lapetite 2023
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
UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual void PrepareForData(FPCGExPointIO& InData, FPCGAttributeAccessorKeysPoints* InPrimaryKeys = nullptr);
	virtual void ProcessPoints(UPCGPointData* InData) const;
	virtual void ProcessSubPoints(const PCGEx::FPointRef& Start, const PCGEx::FPointRef& End, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathInfos& PathInfos) const;
	virtual void ProcessSubPoints(TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathInfos& PathInfos) const;
};
