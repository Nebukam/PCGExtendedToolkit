// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>

#include "CoreMinimal.h"
#include "PCGExEdge.h"
#include "PCGExSortHelpers.h"
#include "Details/PCGExDetailsGraph.h"
#include "Utils/PCGValueRange.h"

class UPCGMetadata;
struct FPCGContext;

namespace PCGExMT
{
	struct FScope;
	class FTaskManager;
	class IAsyncHandleGroup;
}

struct FPCGExCarryOverDetails;
struct FPCGExEdgeUnionMetadataDetails;
struct FPCGExPointUnionMetadataDetails;
struct FPCGExEdgeEdgeIntersectionDetails;
struct FPCGExPointEdgeIntersectionDetails;
struct FPCGExPointPointIntersectionDetails;

namespace PCGExDetails
{
	class FDistances;
}

namespace PCGExData
{
	class FPointIOCollection;
	class FFacade;
	struct FConstPoint;
	struct FMutablePoint;
	class FUnionMetadata;
	template <typename T>
	class TBuffer;
}

namespace PCGExBlending
{
	class FUnionBlender;
}

struct FPCGExBlendingDetails;
struct FPCGExTransformDetails;

namespace PCGExGraph
{
	struct FLink;
	class FGraphBuilder;
	class FSubGraph;

}

namespace PCGExCluster
{
	class FCluster;
}


namespace PCGExGraph
{
	using NodeLinks = TArray<FLink, TInlineAllocator<8>>;

	PCGEX_CTX_STATE(State_PreparingUnion)
	PCGEX_CTX_STATE(State_ProcessingUnion)

	PCGEX_CTX_STATE(State_WritingClusters)
	PCGEX_CTX_STATE(State_ReadyToCompile)
	PCGEX_CTX_STATE(State_Compiling)

	PCGEX_CTX_STATE(State_ProcessingPointEdgeIntersections)
	PCGEX_CTX_STATE(State_ProcessingEdgeEdgeIntersections)

	PCGEX_CTX_STATE(State_Pathfinding)
	PCGEX_CTX_STATE(State_WaitingPathfinding)

	const TSet<FName> ProtectedClusterAttributes = {Attr_PCGExEdgeIdx, Attr_PCGExVtxIdx};

	class FGraph;

#pragma region Graph Utils

	bool BuildIndexedEdges(const TSharedPtr<PCGExData::FPointIO>& EdgeIO, const TMap<uint32, int32>& EndpointsLookup, TArray<FEdge>& OutEdges, const bool bStopOnError = false);

#pragma endregion

#pragma region Graph

	struct PCGEXTENDEDTOOLKIT_API FGraphMetadataDetails
	{
		const FPCGExBlendingDetails* EdgesBlendingDetailsPtr = nullptr;
		const FPCGExCarryOverDetails* EdgesCarryOverDetails = nullptr;

#define PCGEX_FOREACH_POINTPOINT_METADATA(MACRO)\
		MACRO(IsPointUnion, PointUnionData.bWriteIsUnion, PointUnionData.IsUnion, TEXT("bIsUnion"))\
		MACRO(PointUnionSize, PointUnionData.bWriteUnionSize, PointUnionData.UnionSize, TEXT("UnionSize"))\
		MACRO(IsSubEdge, EdgeUnionData.bWriteIsSubEdge, EdgeUnionData.IsSubEdge, TEXT("bIsSubEdge"))\
		MACRO(IsEdgeUnion, EdgeUnionData.bWriteIsUnion, EdgeUnionData.IsUnion, TEXT("bIsUnion"))\
		MACRO(EdgeUnionSize, EdgeUnionData.bWriteUnionSize, EdgeUnionData.UnionSize, TEXT("UnionSize"))

#define PCGEX_FOREACH_POINTEDGE_METADATA(MACRO)\
		MACRO(IsIntersector, bWriteIsIntersector, IsIntersector,TEXT("bIsIntersector"))

#define PCGEX_FOREACH_EDGEEDGE_METADATA(MACRO)\
		MACRO(Crossing, bWriteCrossing, Crossing,TEXT("bCrossing"))

#define PCGEX_GRAPH_META_DECL(_NAME, _ACCESSOR, _ACCESSOR2, _DEFAULT)	bool bWrite##_NAME = false; FName _NAME##AttributeName = _DEFAULT;
		PCGEX_FOREACH_POINTPOINT_METADATA(PCGEX_GRAPH_META_DECL);
		PCGEX_FOREACH_POINTEDGE_METADATA(PCGEX_GRAPH_META_DECL);
		PCGEX_FOREACH_EDGEEDGE_METADATA(PCGEX_GRAPH_META_DECL);

		bool bFlagCrossing = false;
		FName FlagA = NAME_None;
		FName FlagB = NAME_None;

		void Grab(const FPCGContext* Context, const FPCGExPointPointIntersectionDetails& InDetails);
		void Grab(const FPCGContext* Context, const FPCGExPointEdgeIntersectionDetails& InDetails);
		void Grab(const FPCGContext* Context, const FPCGExEdgeEdgeIntersectionDetails& InDetails);
		void Grab(const FPCGContext* Context, const FPCGExPointUnionMetadataDetails& InDetails);
		void Grab(const FPCGContext* Context, const FPCGExEdgeUnionMetadataDetails& InDetails);

#undef PCGEX_FOREACH_POINTPOINT_METADATA
#undef PCGEX_FOREACH_POINTEDGE_METADATA
#undef PCGEX_FOREACH_EDGEEDGE_METADATA
	};

	struct PCGEXTENDEDTOOLKIT_API FGraphNodeMetadata
	{
		int32 NodeIndex;
		int32 UnionSize = 0; // Fuse size
		EPCGExIntersectionType Type;

		explicit FGraphNodeMetadata(const int32 InNodeIndex, const EPCGExIntersectionType InType = EPCGExIntersectionType::Unknown);

		FORCEINLINE bool IsUnion() const { return UnionSize > 1; }
		FORCEINLINE bool IsIntersector() const { return Type == EPCGExIntersectionType::PointEdge; }
		FORCEINLINE bool IsCrossing() const { return Type == EPCGExIntersectionType::EdgeEdge; }
	};

	struct PCGEXTENDEDTOOLKIT_API FGraphEdgeMetadata
	{
		int32 EdgeIndex;
		int32 RootIndex;
		EPCGExIntersectionType Type;

		int32 UnionSize = 0; // Fuse size
		int8 bIsSubEdge = 0; // Sub Edge (result of a)

		FORCEINLINE bool IsUnion() const { return UnionSize > 1; }
		FORCEINLINE bool IsRoot() const { return EdgeIndex == RootIndex; }

		explicit FGraphEdgeMetadata(const int32 InEdgeIndex, const int32 InRootIndex = -1, const EPCGExIntersectionType InType = EPCGExIntersectionType::Unknown);
	};

	struct PCGEXTENDEDTOOLKIT_API FNode
	{
		FNode() = default;

		FNode(const int32 InNodeIndex, const int32 InPointIndex);

		int8 bValid = 1; // int for atomic operations

		int32 Index = -1;      // Index in the context of the list that helds the node
		int32 PointIndex = -1; // Index in the context of the UPCGBasePointData that helds the vtx
		int32 NumExportedEdges = 0;

		NodeLinks Links;

		~FNode() = default;

		FORCEINLINE int32 Num() const { return Links.Num(); }
		FORCEINLINE int32 IsEmpty() const { return Links.IsEmpty(); }

		FORCEINLINE bool IsLeaf() const { return Links.Num() == 1; }
		FORCEINLINE bool IsBinary() const { return Links.Num() == 2; }
		FORCEINLINE bool IsComplex() const { return Links.Num() > 2; }

		FORCEINLINE void LinkEdge(const int32 EdgeIndex) { Links.AddUnique(FLink(0, EdgeIndex)); }
		FORCEINLINE void Link(const int32 NodeIndex, const int32 EdgeIndex) { Links.AddUnique(FLink(NodeIndex, EdgeIndex)); }

		bool IsAdjacentTo(const int32 OtherNodeIndex) const;

		int32 GetEdgeIndex(const int32 AdjacentNodeIndex) const;
	};

	class PCGEXTENDEDTOOLKIT_API FGraph : public TSharedFromThis<FGraph>
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

	bool BuildEndpointsLookup(const TSharedPtr<PCGExData::FPointIO>& InPointIO, TMap<uint32, int32>& OutIndices, TArray<int32>& OutAdjacency);

#pragma endregion

	PCGEXTENDEDTOOLKIT_API bool IsPointDataVtxReady(const UPCGMetadata* Metadata);

	PCGEXTENDEDTOOLKIT_API bool IsPointDataEdgeReady(const UPCGMetadata* Metadata);

	PCGEXTENDEDTOOLKIT_API void CleanupVtxData(const TSharedPtr<PCGExData::FPointIO>& PointIO);

	PCGEXTENDEDTOOLKIT_API void CleanupEdgeData(const TSharedPtr<PCGExData::FPointIO>& PointIO);

	PCGEXTENDEDTOOLKIT_API void CleanupClusterData(const TSharedPtr<PCGExData::FPointIO>& PointIO);
}
