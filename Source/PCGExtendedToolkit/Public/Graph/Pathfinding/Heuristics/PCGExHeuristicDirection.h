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
		const PCGExCluster::FScoredVertex* From,
		const PCGExCluster::FVertex& To,
		const PCGExCluster::FVertex& Seed,
		const PCGExCluster::FVertex& Goal,
		const PCGExGraph::FIndexedEdge& Edge) const override;

	virtual bool IsBetterScore(const double NewScore, const double OtherScore) const override;
};
