// Copyright 2025 Timothé Lapetite and contributors
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
		TMap<int32, FGraphNodeMetadata> NodeMetadata;

		TSharedPtr<PCGExData::FUnionMetadata> EdgesUnion;
		TMap<int32, FGraphEdgeMetadata> EdgeMetadata;

		TMap<uint64, int32> UniqueEdges;

		TArray<TSharedRef<FSubGraph>> SubGraphs;
		TSharedPtr<PCGEx::FIndexLookup> NodeIndexLookup;

		bool bRefreshEdgeSeed = false;

		explicit FGraph(const int32 InNumNodes);

		void ReserveForEdges(const int32 UpcomingAdditionCount, bool bReserveMeta = false);

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

		FEdge* FindEdge_Unsafe(const uint64 Hash);
		FEdge* FindEdge_Unsafe(const int32 A, const int32 B);
		FEdge* FindEdge(const uint64 Hash);
		FEdge* FindEdge(const int32 A, const int32 B);

#pragma region metadata

		FORCEINLINE FGraphEdgeMetadata& GetOrCreateEdgeMetadata_Unsafe(const int32 EdgeIndex, const int32 RootIndex = -1)
		{
			return EdgeMetadata.FindOrAdd(EdgeIndex, FGraphEdgeMetadata(EdgeIndex, RootIndex));
		}

		FGraphEdgeMetadata& GetOrCreateEdgeMetadata(const int32 EdgeIndex, const int32 RootIndex = -1);

		FORCEINLINE FGraphNodeMetadata& GetOrCreateNodeMetadata_Unsafe(const int32 NodeIndex)
		{
			return NodeMetadata.FindOrAdd(NodeIndex, FGraphNodeMetadata(NodeIndex));
		}

		FORCEINLINE FGraphEdgeMetadata& AddNodeAndEdgeMetadata_Unsafe(const int32 InNodeIndex, const int32 InEdgeIndex, const int32 RootIndex = -1, const EPCGExIntersectionType InType = EPCGExIntersectionType::Unknown)
		{
			NodeMetadata.FindOrAdd(InNodeIndex, FGraphNodeMetadata(InNodeIndex)).Type = InType;
			return EdgeMetadata.FindOrAdd(InEdgeIndex, FGraphEdgeMetadata(InEdgeIndex, RootIndex, InType));
		}

		FORCEINLINE void AddNodeMetadata_Unsafe(const int32 InNodeIndex, const EPCGExIntersectionType InType)
		{
			NodeMetadata.FindOrAdd(InNodeIndex, FGraphNodeMetadata(InNodeIndex)).Type = InType;
		}

		FORCEINLINE FGraphEdgeMetadata& AddEdgeMetadata_Unsafe(const int32 InEdgeIndex, const int32 RootIndex = -1, const EPCGExIntersectionType InType = EPCGExIntersectionType::Unknown)
		{
			return EdgeMetadata.FindOrAdd(InEdgeIndex, FGraphEdgeMetadata(InEdgeIndex, RootIndex, InType));
		}

		FORCEINLINE const FGraphNodeMetadata* FindNodeMetadata_Unsafe(const int32 NodeIndex)
		{
			return NodeMetadata.Find(NodeIndex);
		}

		FORCEINLINE const FGraphEdgeMetadata* FindEdgeMetadata_Unsafe(const int32 EdgeIndex)
		{
			return EdgeMetadata.Find(EdgeIndex);
		}

		FORCEINLINE int32 FindEdgeMetadataRootIndex_Unsafe(const int32 EdgeIndex)
		{
			if (const FGraphEdgeMetadata* E = EdgeMetadata.Find(EdgeIndex)) { return E->RootIndex; }
			return -1;
		}

#pragma endregion

		TArrayView<FNode> AddNodes(const int32 NumNewNodes, int32& OutStartIndex);

		void BuildSubGraphs(const FPCGExGraphBuilderDetails& Limits, TArray<int32>& OutValidNodes);

		~FGraph() = default;

		void GetConnectedNodes(int32 FromIndex, TArray<int32>& OutIndices, int32 SearchDepth) const;
	};
}
