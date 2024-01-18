// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Graph/PCGExCluster.h"
#include "PCGExHeuristicDistance.h"

#include "PCGExHeuristicNodeCount.generated.h"
/**
 * 
 */
UCLASS(DisplayName = "Least Nodes")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicNodeCount : public UPCGExHeuristicDistance
{
	GENERATED_BODY()

public:
	virtual double GetEdgeScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& To,
		const PCGExGraph::FIndexedEdge& Edge,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override;
};
