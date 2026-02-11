// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGraphMetadata.h"
#include "Clusters/PCGExEdge.h"
#include "Graphs/PCGExGraphDetails.h"
#include "Clusters/PCGExNode.h"

namespace PCGEx
{
	class FIndexLookup;
}

namespace PCGExData
{
	class FUnionMetadata;
}

namespace PCGExGraphs
{
	class FSubGraph;

	class PCGEXGRAPHS_API FGraph : public TSharedFromThis<FGraph>
	{
		mutable FRWLock GraphLock;
		mutable FRWLock MetadataLock;

	public:
		bool bBuildClusters = false;

		TArray<FNode> Nodes;
		TArray<FEdge> Edges;

		TSharedPtr<PCGExData::FUnionMetadata> NodesUnion;
		TArray<FGraphNodeMetadata> NodeMetadata;
		bool bHasAnyNodeMetadata = false;

		TSharedPtr<PCGExData::FUnionMetadata> EdgesUnion;
		TArray<FGraphEdgeMetadata> EdgeMetadata;
		bool bHasAnyEdgeMetadata = false;

		TMap<uint64, int32> UniqueEdges;

		TArray<TSharedRef<FSubGraph>> SubGraphs;
		TSharedPtr<PCGEx::FIndexLookup> NodeIndexLookup;

		bool bRefreshEdgeSeed = false;

		explicit FGraph(const int32 InNumNodes);

		void ReserveForEdges(const int32 UpcomingAdditionCount);

		bool InsertEdge_Unsafe(int32 A, int32 B, FEdge& OutEdge, int32 IOIndex);
		bool InsertEdge(const int32 A, const int32 B, FEdge& OutEdge, const int32 IOIndex = -1);

		bool InsertEdge_Unsafe(const FEdge& Edge);
		bool InsertEdge(const FEdge& Edge);
		bool InsertEdge_Unsafe(const FEdge& Edge, FEdge& OutEdge, const int32 InIOIndex);
		bool InsertEdge(const FEdge& Edge, FEdge& OutEdge, const int32 InIOIndex);

		void InsertEdges_Unsafe(const TSet<uint64>& InEdges, int32 InIOIndex);
		void InsertEdges(const TSet<uint64>& InEdges, int32 InIOIndex);

		void InsertEdges(const TArray<uint64>& InEdges, int32 InIOIndex);
		int32 InsertEdges(const TArray<FEdge>& InEdges);

		/** Bulk-adopt pre-deduplicated edges without hash checking. Edges are guaranteed unique from FUnionGraph. */
		void AdoptEdges(TArray<FEdge>& InEdges);

		FEdge* FindEdge_Unsafe(const uint64 Hash);
		FEdge* FindEdge_Unsafe(const int32 A, const int32 B);
		FEdge* FindEdge(const uint64 Hash);
		FEdge* FindEdge(const int32 A, const int32 B);

#pragma region metadata

		FORCEINLINE bool HasAnyNodeMetadata() const { return bHasAnyNodeMetadata; }
		FORCEINLINE bool HasAnyEdgeMetadata() const { return bHasAnyEdgeMetadata; }

		FORCEINLINE FGraphEdgeMetadata& GetOrCreateEdgeMetadata_Unsafe(const int32 EdgeIndex, const int32 RootIndex = -1)
		{
			if (EdgeMetadata[EdgeIndex].EdgeIndex == -1)
			{
				EdgeMetadata[EdgeIndex] = FGraphEdgeMetadata(EdgeIndex, RootIndex);
				bHasAnyEdgeMetadata = true;
			}
			return EdgeMetadata[EdgeIndex];
		}

		FGraphEdgeMetadata& GetOrCreateEdgeMetadata(const int32 EdgeIndex, const int32 RootIndex = -1);

		FORCEINLINE FGraphNodeMetadata& GetOrCreateNodeMetadata_Unsafe(const int32 NodeIndex)
		{
			if (NodeMetadata[NodeIndex].NodeIndex == -1)
			{
				NodeMetadata[NodeIndex] = FGraphNodeMetadata(NodeIndex);
				bHasAnyNodeMetadata = true;
			}
			return NodeMetadata[NodeIndex];
		}

		FORCEINLINE FGraphNodeMetadata* FindNodeMetadata_Unsafe(const int32 NodeIndex)
		{
			if (NodeIndex < 0 || NodeIndex >= NodeMetadata.Num() || NodeMetadata[NodeIndex].NodeIndex == -1) { return nullptr; }
			return &NodeMetadata[NodeIndex];
		}

		FORCEINLINE FGraphEdgeMetadata* FindEdgeMetadata_Unsafe(const int32 EdgeIndex)
		{
			if (EdgeIndex < 0 || EdgeIndex >= EdgeMetadata.Num() || EdgeMetadata[EdgeIndex].EdgeIndex == -1) { return nullptr; }
			return &EdgeMetadata[EdgeIndex];
		}

		FORCEINLINE int32 FindEdgeMetadataRootIndex_Unsafe(const int32 EdgeIndex)
		{
			if (EdgeIndex < 0 || EdgeIndex >= EdgeMetadata.Num() || EdgeMetadata[EdgeIndex].EdgeIndex == -1) { return -1; }
			return EdgeMetadata[EdgeIndex].RootIndex;
		}

#pragma endregion

		TArrayView<FNode> AddNodes(const int32 NumNewNodes, int32& OutStartIndex);

		void BuildSubGraphs(const FPCGExGraphBuilderDetails& Limits, TArray<int32>& OutValidNodes);

		~FGraph() = default;

		void GetConnectedNodes(int32 FromIndex, TArray<int32>& OutIndices, int32 SearchDepth) const;
	};
}
