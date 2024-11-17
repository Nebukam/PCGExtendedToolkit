// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOperation.h"
#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

#include "PCGExEdgeRefineOperation.generated.h"

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

	TArray<int8>* VtxFilters = nullptr;
	TArray<int8>* EdgesFilters = nullptr;

	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader)
	{
	}

	virtual void PrepareVtxFacade(const TSharedPtr<PCGExData::FFacade>& InVtxFacade) const
	{
	}

	virtual void PrepareForCluster(const TSharedPtr<PCGExCluster::FCluster>& InCluster, const TSharedPtr<PCGExHeuristics::FHeuristicsHandler>& InHeuristics = nullptr)
	{
		Cluster = InCluster;
		Heuristics = InHeuristics;

		if (RequiresNodeOctree()) { Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Node); }
		if (RequiresEdgeOctree()) { Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Edge); }

		if (RequiresHeuristics() && Heuristics)
		{
			RoamingSeedNode = Heuristics->GetRoamingSeed();
			RoamingGoalNode = Heuristics->GetRoamingGoal();
		}
	}

	virtual void Process()
	{
	}

	virtual void ProcessNode(PCGExCluster::FNode& Node)
	{
	}

	virtual void ProcessEdge(PCGExGraph::FEdge& Edge)
	{
	}

	virtual void Cleanup() override
	{
		Cluster.Reset();
		Heuristics.Reset();
		Super::Cleanup();
	}

protected:
	const PCGExCluster::FNode* RoamingSeedNode = nullptr;
	const PCGExCluster::FNode* RoamingGoalNode = nullptr;
	TSharedPtr<PCGExCluster::FCluster> Cluster;
	TSharedPtr<PCGExHeuristics::FHeuristicsHandler> Heuristics;
	mutable FRWLock EdgeLock;
	mutable FRWLock NodeLock;
};
