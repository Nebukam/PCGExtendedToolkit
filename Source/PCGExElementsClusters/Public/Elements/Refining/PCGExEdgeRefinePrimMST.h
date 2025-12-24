// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "Containers/PCGExHashLookup.h"
#include "Utils/PCGExScoredQueue.h"
#include "PCGExEdgeRefinePrimMST.generated.h"

/**
 * 
 */
class FPCGExEdgeRefinePrimMST : public FPCGExEdgeRefineOperation
{
public:
	virtual void Process() override
	{
		const int32 NumNodes = Cluster->Nodes->Num();

		TBitArray<> Visited;
		Visited.Init(false, NumNodes);

		const PCGExClusters::FNode& RoamingSeedNode = *Heuristics->GetRoamingSeed();
		const PCGExClusters::FNode& RoamingGoalNode = *Heuristics->GetRoamingGoal();

		const TUniquePtr<PCGEx::FScoredQueue> ScoredQueue = MakeUnique<PCGEx::FScoredQueue>(NumNodes);
		ScoredQueue->Enqueue(RoamingSeedNode.Index, 0);
		const TSharedPtr<PCGEx::FHashLookup> TravelStack = PCGEx::NewHashLookup<PCGEx::FHashLookupArray>(PCGEx::NH64(-1, -1), NumNodes);

		int32 CurrentNodeIndex;
		double CurrentNodeScore;
		while (ScoredQueue->Dequeue(CurrentNodeIndex, CurrentNodeScore))
		{
			const PCGExClusters::FNode& Current = *Cluster->GetNode(CurrentNodeIndex);
			Visited[CurrentNodeIndex] = true;

			for (const PCGExGraphs::FLink Lk : Current.Links)
			{
				const uint32 NeighborIndex = Lk.Node;
				const uint32 EdgeIndex = Lk.Edge;

				if (Visited[NeighborIndex]) { continue; } // Exit early

				const PCGExClusters::FNode& AdjacentNode = *Cluster->GetNode(NeighborIndex);
				PCGExGraphs::FEdge& Edge = *Cluster->GetEdge(EdgeIndex);

				const double Score = Heuristics->GetEdgeScore(Current, AdjacentNode, Edge, RoamingSeedNode, RoamingGoalNode, nullptr, TravelStack);
				if (!ScoredQueue->Enqueue(NeighborIndex, Score)) { continue; }

				TravelStack->Set(NeighborIndex, PCGEx::NH64(CurrentNodeIndex, EdgeIndex));
			}
		}

		for (int32 i = 0; i < NumNodes; i++)
		{
			int32 Node;
			int32 EdgeIndex;

			PCGEx::NH64(TravelStack->Get(i), Node, EdgeIndex);
			if (Node == -1 || EdgeIndex == -1) { continue; }

			Cluster->GetEdge(EdgeIndex)->bValid = !bInvert;
		}
	}

	bool bInvert = false;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Refine : MST (Prim)", PCGExNodeLibraryDoc="clusters/refine-cluster/mst-prim"))
class UPCGExEdgeRefinePrimMST : public UPCGExEdgeRefineInstancedFactory
{
	GENERATED_BODY()

public:
	virtual bool GetDefaultEdgeValidity() const override { return bInvert; }
	virtual bool WantsHeuristics() const override { return true; }

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExEdgeRefinePrimMST* TypedOther = Cast<UPCGExEdgeRefinePrimMST>(Other))
		{
			bInvert = TypedOther->bInvert;
		}
	}

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;

	PCGEX_CREATE_REFINE_OPERATION(EdgeRefinePrimMST, { Operation->bInvert = bInvert; })
};
