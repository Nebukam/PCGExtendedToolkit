// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGPoint.h"
#include "Data/Blending/PCGExPropertiesBlender.h"
#include "Paths/SubPoints/PCGExSubPointsOperation.h"
#include "PCGExSubPointsBlendOperation.generated.h"

namespace PCGExDataBlending
{
	class FMetadataBlender;
}

/**
 * 
 */
UCLASS(Abstract)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSubPointsBlendOperation : public UPCGExSubPointsOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExBlendingDetails BlendingDetails = FPCGExBlendingDetails(EPCGExDataBlendingType::Lerp);

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual void PrepareForData(PCGExData::FFacade* InPrimaryFacade) override;
	virtual void PrepareForData(PCGExData::FFacade* InPrimaryFacade, PCGExData::FFacade* InSecondaryFacade, const PCGExData::ESource SecondarySource);

	virtual void ProcessSubPoints(
		const PCGExData::FPointRef& From,
		const PCGExData::FPointRef& To,
		const TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetricsSquared& Metrics,
		const int32 StartIndex = -1) const override;

	virtual void BlendSubPoints(
		const PCGExData::FPointRef& From,
		const PCGExData::FPointRef& To,
		const TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetricsSquared& Metrics,
		PCGExDataBlending::FMetadataBlender* InBlender,
		const int32 StartIndex = -1) const;

	virtual void BlendSubPoints(
		TArray<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetricsSquared& Metrics,
		PCGExDataBlending::FMetadataBlender* InBlender) const;

	virtual void Cleanup() override;

	virtual PCGExDataBlending::FMetadataBlender* CreateBlender(
		PCGExData::FFacade* InPrimaryFacade,
		PCGExData::FFacade* InSecondaryFacade,
		const PCGExData::ESource SecondarySource = PCGExData::ESource::In);

protected:
	virtual EPCGExDataBlendingType GetDefaultBlending();
	PCGExDataBlending::FMetadataBlender* InternalBlender;
};
