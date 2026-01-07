// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsBlendOperation.h"


#include "PCGExSubPointsBlendNone.generated.h"

class FPCGExSubPointsBlendNone : public FPCGExSubPointsBlendOperation
{
public:
	virtual bool PrepareForData(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InTargetFacade, const TSharedPtr<PCGExData::FFacade>& InSourceFacade, const PCGExData::EIOSide InSourceSide, const TSet<FName>* IgnoreAttributeSet = nullptr) override;

	virtual void BlendSubPoints(const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To, PCGExData::FScope& Scope, const PCGExPaths::FPathMetrics& Metrics) const override;
};

/**
 * 
 */
UCLASS(MinimalAPI, meta=(DisplayName = "No Blending", PCGExNodeLibraryDoc="paths/sub-point-blending/none"))
class UPCGExSubPointsBlendNone : public UPCGExSubPointsBlendInstancedFactory
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExSubPointsBlendOperation> CreateOperation() const override;
};
