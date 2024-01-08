// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExCluster.h"
#include "UObject/Object.h"
#include "PCGExHeuristicOperation.h"
#include "PCGExHeuristicDistance.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Distance")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicDistance : public UPCGExHeuristicOperation
{
	GENERATED_BODY()

public:
	virtual double ComputeScore(
		const PCGExCluster::FScoredVertex* From,
		const PCGExCluster::FVertex& To,
		const PCGExCluster::FVertex& Seed,
		const PCGExCluster::FVertex& Goal,
		const PCGExCluster::FIndexedEdge& Edge) const override;

	virtual bool IsBetterScore(const double NewScore, const double OtherScore) const override;
};
