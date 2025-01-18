// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGPoint.h"
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

	virtual void PrepareForData(const TSharedPtr<PCGExData::FFacade>& InPrimaryFacade, const TSet<FName>* IgnoreAttributeSet = nullptr) override;
	virtual void PrepareForData(const TSharedPtr<PCGExData::FFacade>& InPrimaryFacade, const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade, const PCGExData::ESource SecondarySource, const TSet<FName>* IgnoreAttributeSet = nullptr);

	virtual void ProcessSubPoints(
		const PCGExData::FPointRef& From,
		const PCGExData::FPointRef& To,
		const TArrayView<FPCGPoint>& SubPoints,
		const PCGExPaths::FPathMetrics& Metrics,
		const int32 StartIndex = -1) const override;

	virtual void BlendSubPoints(
		const PCGExData::FPointRef& From,
		const PCGExData::FPointRef& To,
		const TArrayView<FPCGPoint>& SubPoints,
		const PCGExPaths::FPathMetrics& Metrics,
		PCGExDataBlending::FMetadataBlender* InBlender,
		const int32 StartIndex = -1) const;

	virtual void BlendSubPoints(
		TArray<FPCGPoint>& SubPoints,
		const PCGExPaths::FPathMetrics& Metrics,
		PCGExDataBlending::FMetadataBlender* InBlender) const;

	virtual void Cleanup() override;

	virtual TSharedPtr<PCGExDataBlending::FMetadataBlender> CreateBlender(
		const TSharedRef<PCGExData::FFacade>& InPrimaryFacade,
		const TSharedRef<PCGExData::FFacade>& InSecondaryFacade,
		const PCGExData::ESource SecondarySource = PCGExData::ESource::In,
		const TSet<FName>* IgnoreAttributeSet = nullptr);

protected:
	virtual EPCGExDataBlendingType GetDefaultBlending();
	TSharedPtr<PCGExDataBlending::FMetadataBlender> InternalBlender;
};
