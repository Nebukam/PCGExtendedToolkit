// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExHeuristicLocalDistance.h"
#include "Graph/PCGExCluster.h"
#include "UObject/Object.h"
#include "PCGExHeuristicOperation.h"
#include "PCGExHeuristicDistance.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Shortest Distance")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicDistance : public UPCGExHeuristicLocalDistance
{
	GENERATED_BODY()

public:
	virtual void PrepareForData(PCGExCluster::FCluster* InCluster) override;

	virtual double GetGlobalScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override;

protected:
	double MaxDistSquared = 0;
};
