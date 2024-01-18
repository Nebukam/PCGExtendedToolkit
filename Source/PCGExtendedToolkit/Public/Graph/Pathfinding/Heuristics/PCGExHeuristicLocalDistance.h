// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExCluster.h"
#include "UObject/Object.h"
#include "PCGExHeuristicOperation.h"
#include "PCGExHeuristicLocalDistance.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Local Distance (MST)")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicLocalDistance : public UPCGExHeuristicOperation
{
	GENERATED_BODY()

public:
	virtual void PrepareForData(PCGExCluster::FCluster* InCluster) override;

	virtual double GetEdgeScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& To,
		const PCGExGraph::FIndexedEdge& Edge,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override;
};
