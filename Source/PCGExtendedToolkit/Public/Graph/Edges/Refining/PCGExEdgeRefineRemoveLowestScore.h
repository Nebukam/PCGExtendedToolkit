// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "PCGExEdgeRefineRemoveLowestScore.generated.h"

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
UCLASS(BlueprintType, meta=(DisplayName="Remove Lowest Score"))
class PCGEXTENDEDTOOLKIT_API UPCGExEdgeRemoveLowestScore : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual bool RequiresHeuristics() override;
	virtual bool RequiresIndividualNodeProcessing() override;
	virtual void ProcessNode(PCGExCluster::FNode& Node, PCGExCluster::FCluster* InCluster, FRWLock& EdgeLock, PCGExHeuristics::THeuristicsHandler* InHeuristics) override;
};
