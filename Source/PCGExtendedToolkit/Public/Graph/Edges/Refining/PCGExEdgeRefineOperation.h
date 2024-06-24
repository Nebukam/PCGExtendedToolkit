// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOperation.h"
#include "PCGExEdgeRefineOperation.generated.h"

namespace PCGExGraph
{
	struct FIndexedEdge;
}

namespace PCGExCluster
{
	struct FNode;
}

namespace PCGExHeuristics
{
	class THeuristicsHandler;
}

namespace PCGExGraph
{
	class FGraph;
}

namespace PCGExCluster
{
	struct FCluster;
}

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExEdgeRefineOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual bool RequiresHeuristics();
	virtual bool RequiresIndividualNodeProcessing();
	virtual bool RequiresIndividualEdgeProcessing();
	virtual void Process(PCGExCluster::FCluster* InCluster, PCGExHeuristics::THeuristicsHandler* InHeuristics = nullptr);
	virtual void ProcessNode(PCGExCluster::FNode& Node, PCGExCluster::FCluster* InCluster, FRWLock& EdgeLock, PCGExHeuristics::THeuristicsHandler* InHeuristics = nullptr);
	virtual void ProcessEdge(PCGExGraph::FIndexedEdge& Edge, PCGExCluster::FCluster* InCluster, FRWLock& NodeLock, PCGExHeuristics::THeuristicsHandler* InHeuristics = nullptr);
	virtual void Cleanup() override;
};
