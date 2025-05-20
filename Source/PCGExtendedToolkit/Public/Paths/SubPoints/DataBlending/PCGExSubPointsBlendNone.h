// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsBlendOperation.h"
#include "Editor/Experimental/EditorInteractiveToolsFramework/Public/Behaviors/2DViewportBehaviorTargets.h"
#include "Editor/Experimental/EditorInteractiveToolsFramework/Public/Behaviors/2DViewportBehaviorTargets.h"


#include "PCGExSubPointsBlendNone.generated.h"

class FPCGExSubPointsBlendNone : public FPCGExSubPointsBlendOperation
{
public:
	virtual bool PrepareForData(
		FPCGExContext* InContext,
		const TSharedPtr<PCGExData::FFacade>& InTargetFacade, const TSharedPtr<PCGExData::FFacade>& InSourceFacade,
		const PCGExData::EIOSide InSourceSide, const TSet<FName>* IgnoreAttributeSet = nullptr) override;

	virtual void BlendSubPoints(
		const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To,
		const TArrayView<FPCGPoint>& SubPoints, const PCGExPaths::FPathMetrics& Metrics, const int32 StartIndex = -1) const override;
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "No Blending")
class UPCGExSubPointsBlendNone : public UPCGExSubPointsBlendInstancedFactory
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExSubPointsBlendOperation> CreateOperation() const override;
};
