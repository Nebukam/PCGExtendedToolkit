// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGPoint.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Data/Blending/PCGExPropertiesBlender.h"
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ShowOnlyInnerProperties))
	FPCGExBlendingSettings BlendingSettings;

	virtual void PrepareForData(const UPCGExPointIO* InData) override;
	virtual void PrepareForData(const UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData);
	virtual void ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathInfos& PathInfos) const override;
	virtual void BlendSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathInfos& PathInfos, const UPCGExMetadataBlender* InBlender) const;

	UPCGExMetadataBlender* CreateBlender(const UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData);

	virtual void BeginDestroy() override;

protected:
	virtual EPCGExDataBlendingType GetDefaultBlending();
	UPCGExMetadataBlender* InternalBlender;
	PCGExDataBlending::FPropertiesBlender PropertiesBlender;
};
