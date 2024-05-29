// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "Graph/Pathfinding/PCGExPathfinding.h"
#include "PCGExEdgeRefinePrimMST.generated.h"

class UPCGExHeuristicLocalDistance;
class UPCGExHeuristicDistance;

namespace PCGExCluster
{
	struct FScoredNode;
	struct FNode;
}

/**
 * 
 */
UCLASS(BlueprintType, meta=(DisplayName="MST (Prim)"))
class PCGEXTENDEDTOOLKIT_API UPCGExEdgeRefinePrimMST : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual void Process(PCGExCluster::FCluster* InCluster, PCGExGraph::FGraph* InGraph, PCGExData::FPointIO* InEdgesIO, PCGExHeuristics::THeuristicsHandler* InHeuristics) override;
};
