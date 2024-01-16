// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExHeuristicOperation.h"
#include "Graph/PCGExCluster.h"
#include "PCGExHeuristicDirection.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Direction")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicDirection : public UPCGExHeuristicOperation
{
	GENERATED_BODY()

public:
	virtual double ComputeScore(
		const PCGExCluster::FScoredNode* From,
		const PCGExCluster::FNode& To,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal,
		const PCGExGraph::FIndexedEdge& Edge) const override;

};
