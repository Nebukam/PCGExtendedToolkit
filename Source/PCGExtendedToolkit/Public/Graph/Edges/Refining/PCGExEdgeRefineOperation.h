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
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeRefineOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual bool SupportFilters() { return false; }
	virtual bool GetDefaultEdgeValidity() { return true; }
	virtual bool RequiresNodeOctree() { return false; }
	virtual bool RequiresEdgeOctree() { return false; }
	virtual bool RequiresHeuristics() { return false; }
	virtual bool RequiresIndividualNodeProcessing() { return false; }
	virtual bool RequiresIndividualEdgeProcessing() { return false; }

	TArray<bool>* VtxFilters = nullptr;
	TArray<bool>* EdgesFilters = nullptr;

	virtual void PrepareForCluster(PCGExCluster::FCluster* InCluster, PCGExHeuristics::THeuristicsHandler* InHeuristics = nullptr)
	{
		Cluster = InCluster;
		Heuristics = InHeuristics;

		if (RequiresNodeOctree()) { Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Node); }
		if (RequiresEdgeOctree()) { Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Edge); }
	}

	virtual void Process()
	{
	}

	virtual void ProcessNode(PCGExCluster::FNode& Node)
	{
	}

	virtual void ProcessEdge(PCGExGraph::FIndexedEdge& Edge)
	{
	}

	virtual void Cleanup() override
	{
		Super::Cleanup();
	}

protected:
	PCGExCluster::FCluster* Cluster = nullptr;
	PCGExHeuristics::THeuristicsHandler* Heuristics = nullptr;
	mutable FRWLock EdgeLock;
	mutable FRWLock NodeLock;
};
