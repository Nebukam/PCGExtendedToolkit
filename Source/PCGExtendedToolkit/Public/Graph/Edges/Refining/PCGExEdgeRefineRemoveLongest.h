// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "PCGExEdgeRefineRemoveLongest.generated.h"

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
UCLASS(BlueprintType, meta=(DisplayName="Remove Longest"))
class PCGEXTENDEDTOOLKIT_API UPCGExEdgeRemoveLongest : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual bool RequiresIndividualNodeProcessing() override;
	virtual void ProcessNode(PCGExCluster::FNode& Node, PCGExCluster::FCluster* InCluster, FRWLock& EdgeLock, PCGExHeuristics::THeuristicsHandler* InHeuristics) override;
};
