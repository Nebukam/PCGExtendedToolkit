// Copyright 2025 Timothé Lapetite and contributors
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
class UPCGExEdgeRefineOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual bool SupportFilters() { return false; }
	virtual bool GetDefaultEdgeValidity() { return true; }
	virtual bool WantsNodeOctree() { return false; }
	virtual bool WantsEdgeOctree() { return false; }
	virtual bool WantsHeuristics() { return false; }
	virtual bool WantsIndividualNodeProcessing() { return false; }
	virtual bool WantsIndividualEdgeProcessing() { return false; }

	TArray<int8>* VtxFilters = nullptr;
	TArray<int8>* EdgeFilterCache = nullptr;

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

		if (WantsNodeOctree()) { Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Vtx); }
		if (WantsEdgeOctree()) { Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Edge); }

		if (WantsHeuristics() && Heuristics)
		{
			Heuristics->GetRoamingSeed();
			Heuristics->GetRoamingGoal();
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
	TSharedPtr<PCGExCluster::FCluster> Cluster;
	TSharedPtr<PCGExHeuristics::FHeuristicsHandler> Heuristics;
	mutable FRWLock EdgeLock;
	mutable FRWLock NodeLock;
};
