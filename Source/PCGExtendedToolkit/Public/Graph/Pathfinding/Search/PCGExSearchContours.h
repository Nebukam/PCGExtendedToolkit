﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSearchOperation.h"
#include "Geometry/PCGExGeo.h"
#include "UObject/Object.h"
#include "PCGExSearchContours.generated.h"

namespace PCGExCluster
{
	struct FCluster;
}

struct FPCGExHeuristicModifiersSettings;
class UPCGExHeuristicOperation;
/**
 * 
 */
UCLASS(DisplayName = "Contours", meta=(ToolTip ="Search for contours. Ignores weights, and requires Seed & Goals positions to be strategically placed to yield decent results."))
class PCGEXTENDEDTOOLKIT_API UPCGExSearchContours : public UPCGExSearchOperation
{
	GENERATED_BODY()

public:
	/** Drives how the seed nodes are selected within the graph. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExClusterSearchOrientationMode OrientationMode = EPCGExClusterSearchOrientationMode::CW;

	virtual bool GetRequiresProjection() override;
	virtual bool FindPath(
		const FVector& SeedPosition,
		const FVector& GoalPosition,
		const UPCGExHeuristicOperation* Heuristics,
		const FPCGExHeuristicModifiersSettings* Modifiers,
		TArray<int32>& OutPath,
		PCGExPathfinding::FExtraWeights* ExtraWeights) override;
};
