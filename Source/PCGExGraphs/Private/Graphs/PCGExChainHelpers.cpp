// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graphs/PCGExChainHelpers.h"

#include "Data/PCGExPointIO.h"
#include "Core/PCGExUnionData.h"
#include "Clusters/PCGExCluster.h"
#include "Clusters/Artifacts/PCGExChain.h"
#include "Graphs/PCGExGraph.h"

namespace PCGExClusters::ChainHelpers
{
	void Dump(
		const TSharedRef<FNodeChain>& Chain,
		const TSharedRef<FCluster>& Cluster,
		const TSharedPtr<PCGExGraphs::FGraph>& Graph,
		const bool bAddMetadata)
	{
		const int32 IOIndex = Cluster->EdgesIO.Pin()->IOIndex;
		FEdge OutEdge = FEdge{};

		if (Chain->SingleEdge != -1)
		{
			Graph->InsertEdge(*Cluster->GetEdge(Chain->Seed.Edge), OutEdge, IOIndex);
			if (bAddMetadata) { Graph->GetOrCreateEdgeMetadata(OutEdge.Index).UnionSize = 1; }
		}
		else
		{
			if (bAddMetadata)
			{
				if (Chain->bIsClosedLoop)
				{
					Graph->InsertEdge(*Cluster->GetEdge(Chain->Seed.Edge), OutEdge, IOIndex);
					Graph->GetOrCreateEdgeMetadata(OutEdge.Index).UnionSize = 1;
				}

				for (const FLink& Link : Chain->Links)
				{
					Graph->InsertEdge(*Cluster->GetEdge(Link.Edge), OutEdge, IOIndex);
					Graph->GetOrCreateEdgeMetadata(OutEdge.Index).UnionSize = 1;
				}
			}
			else
			{
				if (Chain->bIsClosedLoop) { Graph->InsertEdge(*Cluster->GetEdge(Chain->Seed.Edge), OutEdge, IOIndex); }
				for (const FLink& Link : Chain->Links) { Graph->InsertEdge(*Cluster->GetEdge(Link.Edge), OutEdge, IOIndex); }
			}
		}
	}

	void DumpReduced(
		const TSharedRef<FNodeChain>& Chain,
		const TSharedRef<FCluster>& Cluster,
		const TSharedPtr<PCGExGraphs::FGraph>& Graph,
		const bool bAddMetadata)
	{
		const int32 IOIndex = Cluster->EdgesIO.Pin()->IOIndex;
		FEdge OutEdge = FEdge{};

		if (Chain->SingleEdge != -1)
		{
			const FEdge& OriginalEdge = *Cluster->GetEdge(Chain->SingleEdge);
			Graph->InsertEdge(OriginalEdge, OutEdge, IOIndex);

			PCGExGraphs::FGraphEdgeMetadata& EdgeMetadata = Graph->GetOrCreateEdgeMetadata(OutEdge.Index);
			EdgeMetadata.UnionSize = 1;

			if (Graph->EdgesUnion)
			{
				Graph->EdgesUnion->NewEntryAt_Unsafe(OutEdge.Index)->Add_Unsafe(PCGExData::FPoint(OriginalEdge.Index, IOIndex));
			}
		}
		else
		{
			if (Chain->bIsClosedLoop)
			{
				// TODO : Handle closed loop properly
				Dump(Chain, Cluster, Graph, bAddMetadata);
				return;
			}

			Graph->InsertEdge(Cluster->GetNodePointIndex(Chain->Seed), Cluster->GetNodePointIndex(Chain->Links.Last()), OutEdge, IOIndex);

			PCGExGraphs::FGraphEdgeMetadata& EdgeMetadata = Graph->GetOrCreateEdgeMetadata(OutEdge.Index);
			EdgeMetadata.UnionSize = Chain->Links.Num();

			if (Graph->EdgesUnion)
			{
				TArray<int32> MergedEdges;
				MergedEdges.Reserve(Chain->Links.Num());

				// TODO : Possible missing edge in some edge cases
				for (const FLink& Link : Chain->Links) { MergedEdges.Add(Link.Edge); }

				Graph->EdgesUnion->NewEntryAt_Unsafe(OutEdge.Index)->Add_Unsafe(IOIndex, MergedEdges);
			}
		}
	}
}
