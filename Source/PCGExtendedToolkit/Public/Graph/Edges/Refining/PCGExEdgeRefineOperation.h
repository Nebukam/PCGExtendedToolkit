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
	virtual bool InvalidateAllEdgesBeforeProcessing() { return false; }
	virtual bool RequiresNodeOctree() { return false; }
	virtual bool RequiresEdgeOctree() { return false; }
	virtual bool RequiresHeuristics() { return false; }
	virtual bool RequiresIndividualNodeProcessing() { return false; }
	virtual bool RequiresIndividualEdgeProcessing() { return false; }

	virtual void PrepareForCluster(PCGExCluster::FCluster* InCluster, PCGExHeuristics::THeuristicsHandler* InHeuristics = nullptr);

	virtual void Process()
	{
	}

	virtual void ProcessNode(PCGExCluster::FNode& Node)
	{
	}

	virtual void ProcessEdge(PCGExGraph::FIndexedEdge& Edge)
	{
	}

	virtual void Cleanup() override;

protected:
	PCGExCluster::FCluster* Cluster = nullptr;
	PCGExHeuristics::THeuristicsHandler* Heuristics = nullptr;
	mutable FRWLock EdgeLock;
	mutable FRWLock NodeLock;
};
