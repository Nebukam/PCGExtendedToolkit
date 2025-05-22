// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGPoint.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Paths/SubPoints/PCGExSubPointsInstancedFactory.h"
#include "PCGExSubPointsBlendOperation.generated.h"

#define PCGEX_CREATE_SUBPOINTBLEND_OPERATION(_TYPE)\
PCGEX_FACTORY_NEW_OPERATION(SubPointsBlend##_TYPE)\
NewOperation->Factory = this;\
NewOperation->BlendFactory = this;

class UPCGExSubPointsBlendInstancedFactory;

namespace PCGExDataBlending
{
	class FMetadataBlender;
}

class FPCGExSubPointsBlendOperation : public FPCGExSubPointsOperation
{
public:
	const UPCGExSubPointsBlendInstancedFactory* BlendFactory = nullptr;

	virtual bool PrepareForData(
		FPCGExContext* InContext,
		const TSharedPtr<PCGExData::FFacade>& InTargetFacade, const TSet<FName>* IgnoreAttributeSet = nullptr) override;

	virtual bool PrepareForData(
		FPCGExContext* InContext,
		const TSharedPtr<PCGExData::FFacade>& InTargetFacade, const TSharedPtr<PCGExData::FFacade>& InSourceFacade,
		const PCGExData::EIOSide InSourceSide, const TSet<FName>* IgnoreAttributeSet = nullptr);

	virtual void ProcessSubPoints(
		const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To,
		PCGExData::FScope& Scope, const PCGExPaths::FPathMetrics& Metrics, const int32 StartIndex = -1) const override;

	virtual void BlendSubPoints(
		const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To,
		PCGExData::FScope& Scope, const PCGExPaths::FPathMetrics& Metrics, const int32 StartIndex = -1) const;

	virtual void BlendSubPoints(PCGExData::FScope& Scope, const PCGExPaths::FPathMetrics& Metrics) const;

protected:
	FPCGExBlendingDetails BlendingDetails;
	TSharedPtr<PCGExDataBlending::FMetadataBlender> MetadataBlender;
};

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsBlendInstancedFactory : public UPCGExSubPointsInstancedFactory
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExBlendingDetails BlendingDetails = FPCGExBlendingDetails(EPCGExDataBlendingType::Lerp);

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override;

	virtual TSharedPtr<FPCGExSubPointsBlendOperation> CreateOperation() const
	PCGEX_NOT_IMPLEMENTED_RET(CreateOperation(), nullptr);
	
};
