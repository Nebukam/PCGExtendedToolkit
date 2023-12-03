// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGPoint.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Splines/SubPoints/PCGExSubPointsProcessor.h"
#include "PCGExSubPointsDataBlend.generated.h"

class UPCGExMetadataBlender;
class UPCGExPointIO;

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsDataBlend : public UPCGExSubPointsProcessor
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TMap<FName, EPCGExDataBlendingType> BlendingOverrides;
	
	virtual void PrepareForData(const UPCGExPointIO* InData) override;
	virtual void PrepareForData(const UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData);
	virtual void ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const double PathLength) const override;
	virtual void ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const double PathLength, const UPCGExMetadataBlender* InBlender) const;

	UPCGExMetadataBlender* CreateBlender(const UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData);
	
protected:
	virtual EPCGExDataBlendingType GetDefaultBlending();
	UPCGExMetadataBlender* InternalBlender;
	
};
