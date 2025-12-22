// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "Clusters/PCGExCluster.h"
#include "PCGExEdgeRefineKeepLongest.generated.h"

class UPCGExHeuristicLocalDistance;
class FPCGExHeuristicDistance;

/**
 * 
 */
class FPCGExEdgeKeepLongest : public FPCGExEdgeRefineOperation
{
public:
	virtual void ProcessNode(PCGExClusters::FNode& Node) override
	{
		int32 BestIndex = -1;
		double LongestDist = 0;

		for (const PCGExGraphs::FLink Lk : Node.Links)
		{
			const double Dist = Cluster->GetDistSquared(Node.Index, Lk.Node);
			if (Dist > LongestDist)
			{
				LongestDist = Dist;
				BestIndex = Lk.Edge;
			}
		}

		if (BestIndex == -1) { return; }
		//if (!*(EdgesFilters->GetData() + BestIndex)) { return; }

		FPlatformAtomics::InterlockedExchange(&Cluster->GetEdge(BestIndex)->bValid, 1);
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Keep Longest", PCGExNodeLibraryDoc="clusters/refine-cluster/edge-length"))
class UPCGExEdgeKeepLongest : public UPCGExEdgeRefineInstancedFactory
{
	GENERATED_BODY()

public:
	virtual bool GetDefaultEdgeValidity() const override { return false; }
	virtual bool WantsIndividualNodeProcessing() const override { return true; }

	PCGEX_CREATE_REFINE_OPERATION(EdgeKeepLongest, {})
};
