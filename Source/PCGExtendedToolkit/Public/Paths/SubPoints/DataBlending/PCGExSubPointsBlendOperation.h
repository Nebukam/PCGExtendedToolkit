// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGPoint.h"
#include "Data/Blending/PCGExDataBlending.h"
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
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsBlendOperation : public UPCGExSubPointsOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExBlendingDetails BlendingDetails = FPCGExBlendingDetails(EPCGExDataBlendingType::Lerp);

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override;

	virtual void PrepareForData(const TSharedPtr<PCGExData::FFacade>& InTargetFacade, const TSet<FName>* IgnoreAttributeSet = nullptr) override;
	virtual void PrepareForData(const TSharedPtr<PCGExData::FFacade>& InTargetFacade, const TSharedPtr<PCGExData::FFacade>& InSourceFacade, const PCGExData::EIOSide InSourceSide, const TSet<FName>* IgnoreAttributeSet = nullptr);

	virtual void ProcessSubPoints(
		const PCGExData::FConstPoint& From,
		const PCGExData::FConstPoint& To,
		const TArrayView<FPCGPoint>& SubPoints,
		const PCGExPaths::FPathMetrics& Metrics,
		const int32 StartIndex = -1) const override;

	virtual void BlendSubPoints(
		const PCGExData::FConstPoint& From,
		const PCGExData::FConstPoint& To,
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
		const TSharedRef<PCGExData::FFacade>& InTargetFacade,
		const TSharedRef<PCGExData::FFacade>& InSourceFacade,
		const PCGExData::EIOSide InSourceSide = PCGExData::EIOSide::In,
		const TSet<FName>* IgnoreAttributeSet = nullptr);

protected:
	virtual EPCGExDataBlendingType GetDefaultBlending();
	TSharedPtr<PCGExDataBlending::FMetadataBlender> InternalBlender;
};
