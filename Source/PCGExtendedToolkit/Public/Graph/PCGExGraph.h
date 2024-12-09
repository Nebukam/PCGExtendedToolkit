// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Data/PCGExAttributeHelpers.h"
#include "PCGExGlobalSettings.h"
#include "PCGExMT.h"
#include "PCGExEdge.h"
#include "PCGExDetails.h"
#include "PCGExDetailsIntersection.h"
#include "Data/PCGExData.h"
#include "Data/Blending/PCGExUnionBlender.h"
#include "PCGExGraph.generated.h"

struct FPCGExBlendingDetails;
struct FPCGExTransformDetails;

namespace PCGExGraph
{
	class FGraphBuilder;
	class FSubGraph;

	using SubGraphPostProcessCallback = std::function<void(const TSharedRef<FSubGraph>& InSubGraph)>;
}

namespace PCGExCluster
{
	class FCluster;
}


UENUM()
enum class EPCGExAdjacencyDirectionOrigin : uint8
{
	FromNode     = 0 UMETA(DisplayName = "From Node to Neighbor", Tooltip="..."),
	FromNeighbor = 1 UMETA(DisplayName = "From Neighbor to Node", Tooltip="..."),
};

UENUM()
enum class EPCGExClusterComponentSource : uint8
{
	Vtx  = 0 UMETA(DisplayName = "Point", Tooltip="Value is fetched from the point being evaluated."),
	Edge = 1 UMETA(DisplayName = "Edge", Tooltip="Value is fetched from the edge connecting to the point being evaluated."),
};

UENUM()
enum class EPCGExIntersectionType : uint8
{
	Unknown   = 0 UMETA(DisplayName = "Unknown", ToolTip="Unknown"),
	PointEdge = 1 UMETA(DisplayName = "Point/Edge", ToolTip="Point/Edge Intersection."),
	EdgeEdge  = 2 UMETA(DisplayName = "Edge/Edge", ToolTip="Edge/Edge Intersection."),
	FusedEdge = 3 UMETA(DisplayName = "Fused Edge", ToolTip="Fused Edge Intersection."),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExGraphBuilderDetails
{
	GENERATED_BODY()

	FPCGExGraphBuilderDetails()
	{
	}

	/** Don't output Clusters if they have less points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteEdgePosition = true;

	/** Edge position interpolation between start and end point positions. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable, EditCondition="bWriteEdgePosition"))
	double EdgePosition = 0.5;

	/** Don't output Clusters if they have less points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveSmallClusters = false;

	/** Minimum points threshold */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable, EditCondition="bRemoveSmallClusters", ClampMin=2))
	int32 MinVtxCount = 3;

	/** Minimum edges threshold */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable, EditCondition="bRemoveSmallClusters", ClampMin=2))
	int32 MinEdgeCount = 3;

	/** Don't output Clusters if they have more points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveBigClusters = false;

	/** Maximum points threshold */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable, EditCondition="bRemoveBigClusters", ClampMin=2))
	int32 MaxVtxCount = 500;

	/** Maximum edges threshold */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable, EditCondition="bRemoveBigClusters", ClampMin=2))
	int32 MaxEdgeCount = 500;

	/** Refresh Edge Seed. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable))
	bool bRefreshEdgeSeed = false;

	/** If the use of cached clusters is enabled, output clusters along with the graph data. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable))
	bool bBuildAndCacheClusters = GetDefault<UPCGExGlobalSettings>()->bDefaultBuildAndCacheClusters;

	bool IsValid(const TSharedPtr<PCGExGraph::FSubGraph>& InSubgraph) const;
};

namespace PCGExGraph
{
	const FName SourceProbesLabel = TEXT("Probes");
	const FName OutputProbeLabel = TEXT("Probe");

	const FName SourceFilterGenerators = TEXT("Generator Filters");
	const FName SourceFilterConnectables = TEXT("Connectable Filters");

	const FName SourceGraphsLabel = TEXT("In");
	const FName OutputGraphsLabel = TEXT("Out");

	const FName SourceVerticesLabel = TEXT("Vtx");
	const FName OutputVerticesLabel = TEXT("Vtx");

	const FName SourcePathsLabel = TEXT("Paths");
	const FName OutputPathsLabel = TEXT("Paths");

	const FName Tag_PackedClusterPointCount = FName(PCGEx::PCGExPrefix + TEXT("PackedClusterPointCount"));
	const FName Tag_PackedClusterEdgeCount = FName(PCGEx::PCGExPrefix + TEXT("PackedClusterEdgeCount"));

	const FName SourceSeedsLabel = TEXT("Seeds");
	const FName SourceGoalsLabel = TEXT("Goals");
	const FName SourcePlotsLabel = TEXT("Plots");

	const FName SourceHeuristicsLabel = TEXT("Heuristics");
	const FName OutputHeuristicsLabel = TEXT("Heuristics");
	const FName OutputModifiersLabel = TEXT("Modifiers");

	const FName SourceVtxFiltersLabel = FName("VtxFilters");
	const FName SourceEdgeFiltersLabel = FName("EdgeFilters");

	PCGEX_ASYNC_STATE(State_PreparingUnion)
	PCGEX_ASYNC_STATE(State_ProcessingUnion)

	PCGEX_ASYNC_STATE(State_WritingClusters)
	PCGEX_ASYNC_STATE(State_ReadyToCompile)
	PCGEX_ASYNC_STATE(State_Compiling)

	PCGEX_ASYNC_STATE(State_ProcessingPointEdgeIntersections)
	PCGEX_ASYNC_STATE(State_ProcessingEdgeEdgeIntersections)

	PCGEX_ASYNC_STATE(State_Pathfinding)
	PCGEX_ASYNC_STATE(State_WaitingPathfinding)

	const TSet<FName> ProtectedClusterAttributes = {Tag_EdgeEndpoints, Tag_VtxEndpoint, Tag_ClusterIndex};

	class FGraph;

#pragma region Graph Utils

	static bool BuildIndexedEdges(
		const TSharedPtr<PCGExData::FPointIO>& EdgeIO,
		const TMap<uint32, int32>& EndpointsLookup,
		TArray<FEdge>& OutEdges,
		const bool bStopOnError = false)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExEdge::BuildIndexedEdges-Vanilla);

		const TUniquePtr<PCGExData::TBuffer<int64>> EndpointsBuffer = MakeUnique<PCGExData::TBuffer<int64>>(EdgeIO.ToSharedRef(), Tag_EdgeEndpoints);
		if (!EndpointsBuffer->PrepareRead()) { return false; }

		const TArray<int64>& Endpoints = *EndpointsBuffer->GetInValues().Get();
		const int32 EdgeIOIndex = EdgeIO->IOIndex;

		bool bValid = true;
		const int32 NumEdges = EdgeIO->GetNum();

		PCGEx::InitArray(OutEdges, NumEdges);

		if (!bStopOnError)
		{
			int32 EdgeIndex = 0;

			for (int i = 0; i < NumEdges; i++)
			{
				uint32 A;
				uint32 B;
				PCGEx::H64(Endpoints[i], A, B);

				const int32* StartPointIndexPtr = EndpointsLookup.Find(A);
				const int32* EndPointIndexPtr = EndpointsLookup.Find(B);

				if ((!StartPointIndexPtr || !EndPointIndexPtr)) { continue; }

				OutEdges[EdgeIndex] = FEdge(EdgeIndex, *StartPointIndexPtr, *EndPointIndexPtr, i, EdgeIOIndex);
				EdgeIndex++;
			}

			PCGEx::InitArray(OutEdges, EdgeIndex);
		}
		else
		{
			for (int i = 0; i < NumEdges; i++)
			{
				uint32 A;
				uint32 B;
				PCGEx::H64(Endpoints[i], A, B);

				const int32* StartPointIndexPtr = EndpointsLookup.Find(A);
				const int32* EndPointIndexPtr = EndpointsLookup.Find(B);

				if ((!StartPointIndexPtr || !EndPointIndexPtr))
				{
					bValid = false;
					break;
				}

				OutEdges[i] = FEdge(i, *StartPointIndexPtr, *EndPointIndexPtr, i, EdgeIOIndex);
			}
		}

		return bValid;
	}

#pragma endregion

#pragma region Graph

	struct /*PCGEXTENDEDTOOLKIT_API*/ FGraphMetadataDetails
	{
		const FPCGExBlendingDetails* EdgesBlendingDetailsPtr = nullptr;
		const FPCGExCarryOverDetails* EdgesCarryOverDetails = nullptr;

#define PCGEX_FOREACH_POINTPOINT_METADATA(MACRO)\
		MACRO(IsPointUnion, PointUnionData.bWriteIsUnion, PointUnionData.IsUnion, TEXT("bIsUnion"))\
		MACRO(PointUnionSize, PointUnionData.bWriteUnionSize, PointUnionData.UnionSize, TEXT("UnionSize"))\
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

#define PCGEX_GRAPH_META_FWD(_NAME, _ACCESSOR, _ACCESSOR2, _DEFAULT)	bWrite##_NAME = InDetails._ACCESSOR; _NAME##AttributeName = InDetails._ACCESSOR2##AttributeName; PCGEX_SOFT_VALIDATE_NAME(bWrite##_NAME, _NAME##AttributeName, Context)

		void Grab(const FPCGContext* Context, const FPCGExPointPointIntersectionDetails& InDetails)
		{
			Grab(Context, InDetails.PointUnionData);
			Grab(Context, InDetails.EdgeUnionData);
		}

		void Grab(const FPCGContext* Context, const FPCGExPointEdgeIntersectionDetails& InDetails)
		{
			PCGEX_FOREACH_POINTEDGE_METADATA(PCGEX_GRAPH_META_FWD);
		}

		void Grab(const FPCGContext* Context, const FPCGExEdgeEdgeIntersectionDetails& InDetails)
		{
			PCGEX_FOREACH_EDGEEDGE_METADATA(PCGEX_GRAPH_META_FWD);
		}

		void Grab(const FPCGContext* Context, const FPCGExPointUnionMetadataDetails& InDetails)
		{
			bWriteIsPointUnion = InDetails.bWriteIsUnion;
			IsPointUnionAttributeName = InDetails.IsUnionAttributeName;
			PCGEX_SOFT_VALIDATE_NAME(bWriteIsPointUnion, IsPointUnionAttributeName, Context)

			bWritePointUnionSize = InDetails.bWriteUnionSize;
			PointUnionSizeAttributeName = InDetails.UnionSizeAttributeName;
			PCGEX_SOFT_VALIDATE_NAME(bWritePointUnionSize, PointUnionSizeAttributeName, Context)
		}

		void Grab(const FPCGContext* Context, const FPCGExEdgeUnionMetadataDetails& InDetails)
		{
			bWriteIsEdgeUnion = InDetails.bWriteIsUnion;
			IsEdgeUnionAttributeName = InDetails.IsUnionAttributeName;
			PCGEX_SOFT_VALIDATE_NAME(bWriteIsEdgeUnion, IsEdgeUnionAttributeName, Context)

			bWriteEdgeUnionSize = InDetails.bWriteUnionSize;
			EdgeUnionSizeAttributeName = InDetails.UnionSizeAttributeName;
			PCGEX_SOFT_VALIDATE_NAME(bWriteEdgeUnionSize, EdgeUnionSizeAttributeName, Context);
		}

#undef PCGEX_FOREACH_POINTPOINT_METADATA
#undef PCGEX_GRAPH_META_FWD
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FGraphNodeMetadata
	{
		EPCGExIntersectionType Type = EPCGExIntersectionType::PointEdge;
		int32 NodeIndex;
		int32 UnionSize = 0; // Fuse size
		bool IsUnion() const { return UnionSize > 1; }

		explicit FGraphNodeMetadata(const int32 InNodeIndex)
			: NodeIndex(InNodeIndex)
		{
		}

		bool IsIntersector() const { return Type == EPCGExIntersectionType::PointEdge; }
		bool IsCrossing() const { return Type == EPCGExIntersectionType::EdgeEdge; }
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FGraphEdgeMetadata
	{
		int32 EdgeIndex;
		int32 ParentIndex;
		int32 RootIndex;
		EPCGExIntersectionType Type = EPCGExIntersectionType::Unknown;

		int32 UnionSize = 0; // Fuse size
		bool IsUnion() const { return UnionSize > 1; }

		explicit FGraphEdgeMetadata(const int32 InEdgeIndex, const FGraphEdgeMetadata* Parent)
			: EdgeIndex(InEdgeIndex), ParentIndex(Parent ? Parent->EdgeIndex : InEdgeIndex), RootIndex(Parent ? Parent->RootIndex : InEdgeIndex)
		{
		}
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FNode
	{
		FNode()
		{
		}

		FNode(const int32 InNodeIndex, const int32 InPointIndex):
			Index(InNodeIndex), PointIndex(InPointIndex)
		{
		}

		int8 bValid = 1; // int for atomic operations

		int32 Index = -1;      // Index in the context of the list that helds the node
		int32 PointIndex = -1; // Index in the context of the UPCGPointData that helds the vtx
		int32 NumExportedEdges = 0;

		TArray<FLink> Links;

		~FNode() = default;

		FORCEINLINE int32 Num() const { return Links.Num(); }
		FORCEINLINE int32 IsEmpty() const { return Links.IsEmpty(); }

		FORCEINLINE bool IsLeaf() const { return Links.Num() == 1; }
		FORCEINLINE bool IsBinary() const { return Links.Num() == 2; }
		FORCEINLINE bool IsComplex() const { return Links.Num() > 2; }

		FORCEINLINE void LinkEdge(const int32 EdgeIndex) { Links.AddUnique(FLink(0, EdgeIndex)); }
		FORCEINLINE void Link(const FNode& Neighbor, const int32 EdgeIndex) { Links.Emplace(Neighbor.Index, EdgeIndex); }

		FORCEINLINE bool IsAdjacentTo(const int32 OtherNodeIndex) const
		{
			for (const FLink Lk : Links) { if (Lk.Node == OtherNodeIndex) { return true; } }
			return false;
		}

		FORCEINLINE int32 GetEdgeIndex(const int32 AdjacentNodeIndex) const
		{
			for (const FLink Lk : Links) { if (Lk.Node == AdjacentNodeIndex) { return Lk.Edge; } }
			return -1;
		}
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FSubGraph : public TSharedFromThis<FSubGraph>
	{
	public:
		int64 Id = -1;
		FGraph* ParentGraph = nullptr;
		TSet<int32> Nodes;
		TSet<int32> Edges;
		TSet<int32> EdgesInIOIndices;
		TSharedPtr<PCGExData::FFacade> VtxDataFacade;
		TSharedPtr<PCGExData::FFacade> EdgesDataFacade;
		TArray<FEdge> FlattenedEdges;
		int32 UID = 0;
		SubGraphPostProcessCallback OnSubGraphPostProcess;


		FSubGraph()
		{
			PCGEX_LOG_CTR(FSubGraph)
		}

		~FSubGraph()
		{
			PCGEX_LOG_DTR(FSubGraph)
		}

		FORCEINLINE void Add(const FEdge& Edge, FGraph* InGraph)
		{
			Nodes.Add(Edge.Start);
			Nodes.Add(Edge.End);
			Edges.Add(Edge.Index);
			if (Edge.IOIndex >= 0) { EdgesInIOIndices.Add(Edge.IOIndex); }
		}

		void Invalidate(FGraph* InGraph);
		TSharedPtr<PCGExCluster::FCluster> CreateCluster(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager);
		int32 GetFirstInIOIndex();

		void Compile(
			const TWeakPtr<PCGExMT::FTaskGroup>& InWeakParentTask,
			const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager,
			const TSharedPtr<FGraphBuilder>& InBuilder);

	protected:
		TWeakPtr<PCGExMT::FTaskManager> WeakAsyncManager;
		TWeakPtr<PCGExMT::FTaskGroup> WeakParentTask;
		TWeakPtr<FGraphBuilder> WeakBuilder;

		const FGraphMetadataDetails* MetadataDetails = nullptr;

		TSharedPtr<PCGExDataBlending::FUnionBlender> UnionBlender;
		TSharedPtr<PCGExDetails::FDistances> Distances;

#define PCGEX_FOREACH_EDGE_METADATA(MACRO)\
MACRO(IsEdgeUnion, bool, false, IsUnion()) \
MACRO(EdgeUnionSize, int32, 0, UnionSize)

#define PCGEX_EDGE_METADATA_DECL(_NAME, _TYPE, _DEFAULT, _ACCESSOR) TSharedPtr<PCGExData::TBuffer<_TYPE>> _NAME##Buffer;
		PCGEX_FOREACH_EDGE_METADATA(PCGEX_EDGE_METADATA_DECL)
#undef PCGEX_EDGE_METADATA_DECL

#undef PCGEX_FOREACH_EDGE_METADATA

		void CompileRange(const PCGExMT::FScope& Scope);
		void CompilationComplete();
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FGraph
	{
		mutable FRWLock GraphLock;
		mutable FRWLock EdgeMetadataLock;
		mutable FRWLock NodeMetadataLock;
		const int32 NumEdgesReserve;

	public:
		bool bBuildClusters = false;

		TArray<FNode> Nodes;
		TArray<FEdge> Edges;

		TSharedPtr<PCGExData::FUnionMetadata> NodesUnion;
		TMap<int32, FGraphNodeMetadata> NodeMetadata;

		TSharedPtr<PCGExData::FUnionMetadata> EdgesUnion;
		TMap<int32, FGraphEdgeMetadata> EdgeMetadata;

		TMap<uint64, int32> UniqueEdges;

		TArray<TSharedPtr<FSubGraph>> SubGraphs;
		TSharedPtr<PCGEx::FIndexLookup> NodeIndexLookup;

		bool bWriteEdgePosition = true;
		double EdgePosition = 0.5;

		bool bRefreshEdgeSeed = false;

		explicit FGraph(const int32 InNumNodes, const int32 InNumEdgesReserve = 10)
			: NumEdgesReserve(InNumEdgesReserve)
		{
			PCGEX_LOG_CTR(FGraph)

			PCGEx::InitArray(Nodes, InNumNodes);

			for (int i = 0; i < InNumNodes; i++)
			{
				FNode& Node = Nodes[i];
				Node.Index = Node.PointIndex = i;
				Node.Links.Reserve(NumEdgesReserve);
			}
		}

		void ReserveForEdges(const int32 UpcomingAdditionCount);

		bool InsertEdgeUnsafe(int32 A, int32 B, FEdge& OutEdge, int32 IOIndex);
		bool InsertEdge(const int32 A, const int32 B, FEdge& OutEdge, const int32 IOIndex = -1);

		bool InsertEdgeUnsafe(const FEdge& Edge);
		bool InsertEdge(const FEdge& Edge);
		bool InsertEdgeUnsafe(const FEdge& Edge, FEdge& OutEdge, const int32 InIOIndex);
		bool InsertEdge(const FEdge& Edge, FEdge& OutEdge, const int32 InIOIndex);

		void InsertEdgesUnsafe(const TSet<uint64>& InEdges, int32 InIOIndex);
		void InsertEdges(const TSet<uint64>& InEdges, int32 InIOIndex);

		void InsertEdges(const TArray<uint64>& InEdges, int32 InIOIndex);
		int32 InsertEdges(const TArray<FEdge>& InEdges);

		FORCEINLINE FEdge* FindEdgeUnsafe(const uint64 Hash)
		{
			const int32* Index = UniqueEdges.Find(Hash);
			if (!Index) { return nullptr; }
			return (Edges.GetData() + *Index);
		}

		FORCEINLINE FEdge* FindEdgeUnsafe(const int32 A, const int32 B) { return FindEdge(PCGEx::H64U(A, B)); }

		FORCEINLINE FEdge* FindEdge(const uint64 Hash)
		{
			FReadScopeLock ReadScopeLock(GraphLock);
			const int32* Index = UniqueEdges.Find(Hash);
			if (!Index) { return nullptr; }
			return (Edges.GetData() + *Index);
		}

		FORCEINLINE FEdge* FindEdge(const int32 A, const int32 B) { return FindEdge(PCGEx::H64U(A, B)); }

		FORCEINLINE FGraphEdgeMetadata& GetOrCreateEdgeMetadataUnsafe(const int32 EdgeIndex, const FGraphEdgeMetadata* Parent = nullptr)
		{
			if (FGraphEdgeMetadata* MetadataPtr = EdgeMetadata.Find(EdgeIndex)) { return *MetadataPtr; }
			return EdgeMetadata.Add(EdgeIndex, FGraphEdgeMetadata(EdgeIndex, Parent));
		}

		FORCEINLINE FGraphEdgeMetadata& GetOrCreateEdgeMetadata(const int32 EdgeIndex, const FGraphEdgeMetadata* Parent = nullptr)
		{
			{
				FReadScopeLock ReadScopeLock(EdgeMetadataLock);
				if (FGraphEdgeMetadata* MetadataPtr = EdgeMetadata.Find(EdgeIndex)) { return *MetadataPtr; }
			}
			{
				FWriteScopeLock WriteScopeLock(EdgeMetadataLock);
				if (FGraphEdgeMetadata* MetadataPtr = EdgeMetadata.Find(EdgeIndex)) { return *MetadataPtr; }
				return EdgeMetadata.Add(EdgeIndex, FGraphEdgeMetadata(EdgeIndex, Parent));
			}
		}

		FORCEINLINE FGraphNodeMetadata& GetOrCreateNodeMetadataUnsafe(const int32 NodeIndex)
		{
			if (FGraphNodeMetadata* MetadataPtr = NodeMetadata.Find(NodeIndex)) { return *MetadataPtr; }
			return NodeMetadata.Add(NodeIndex, FGraphNodeMetadata(NodeIndex));
		}

		FORCEINLINE FGraphNodeMetadata& GetOrCreateNodeMetadata(const int32 NodeIndex)
		{
			{
				FReadScopeLock ReadScopeLock(NodeMetadataLock);
				if (FGraphNodeMetadata* MetadataPtr = NodeMetadata.Find(NodeIndex)) { return *MetadataPtr; }
			}
			{
				FWriteScopeLock WriteScopeLock(NodeMetadataLock);
				if (FGraphNodeMetadata* MetadataPtr = NodeMetadata.Find(NodeIndex)) { return *MetadataPtr; }
				return NodeMetadata.Add(NodeIndex, FGraphNodeMetadata(NodeIndex));
			}
		}

		FORCEINLINE void AddNodeAndEdgeMetadata(
			const int32 InNodeIndex,
			const int32 InEdgeIndex,
			const FGraphEdgeMetadata* InParentMetadata,
			const EPCGExIntersectionType InType)
		{
			FGraphNodeMetadata& N = GetOrCreateNodeMetadataUnsafe(InNodeIndex);
			N.Type = InType;

			FGraphEdgeMetadata& E = GetOrCreateEdgeMetadataUnsafe(InEdgeIndex, InParentMetadata);
			E.Type = InType;
		}

		FORCEINLINE void AddNodeMetadata(
			const int32 InNodeIndex,
			const FGraphEdgeMetadata* InParentMetadata,
			const EPCGExIntersectionType InType)
		{
			FGraphNodeMetadata& N = GetOrCreateNodeMetadataUnsafe(InNodeIndex);
			N.Type = InType;
		}

		FORCEINLINE void AddEdgeMetadata(
			const int32 InEdgeIndex,
			const FGraphEdgeMetadata* InParentMetadata,
			const EPCGExIntersectionType InType)
		{
			FGraphEdgeMetadata& E = GetOrCreateEdgeMetadataUnsafe(InEdgeIndex, InParentMetadata);
			E.Type = InType;
		}

		FORCEINLINE FGraphNodeMetadata* FindNodeMetadataUnsafe(const int32 NodeIndex) { return NodeMetadata.Find(NodeIndex); }
		FORCEINLINE FGraphEdgeMetadata* FindEdgeMetadataUnsafe(const int32 EdgeIndex) { return EdgeMetadata.Find(EdgeIndex); }
		FORCEINLINE FGraphEdgeMetadata* FindRootEdgeMetadataUnsafe(const int32 EdgeIndex)
		{
			const FGraphEdgeMetadata* BaseEdge = EdgeMetadata.Find(EdgeIndex);
			return BaseEdge ? EdgeMetadata.Find(BaseEdge->RootIndex) : nullptr;
		}

		FORCEINLINE FGraphNodeMetadata* FindNodeMetadata(const int32 NodeIndex)
		{
			FReadScopeLock ReadScopeLock(NodeMetadataLock);
			return NodeMetadata.Find(NodeIndex);
		}

		FORCEINLINE FGraphEdgeMetadata* FindEdgeMetadata(const int32 EdgeIndex)
		{
			FReadScopeLock ReadScopeLock(EdgeMetadataLock);
			return EdgeMetadata.Find(EdgeIndex);
		}

		FORCEINLINE FGraphEdgeMetadata* FindRootEdgeMetadata(const int32 EdgeIndex)
		{
			FReadScopeLock ReadScopeLock(EdgeMetadataLock);
			const FGraphEdgeMetadata* BaseEdge = EdgeMetadata.Find(EdgeIndex);
			return BaseEdge ? EdgeMetadata.Find(BaseEdge->RootIndex) : nullptr;
		}


		TArrayView<FNode> AddNodes(const int32 NumNewNodes);

		void BuildSubGraphs(const FPCGExGraphBuilderDetails& Limits);

		~FGraph()
		{
			PCGEX_LOG_DTR(FGraph)
		}

		void GetConnectedNodes(int32 FromIndex, TArray<int32>& OutIndices, int32 SearchDepth) const;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FGraphBuilder : public TSharedFromThis<FGraphBuilder>
	{
	protected:
		TSharedPtr<PCGExMT::FTaskManager> AsyncManager;
		const FGraphMetadataDetails* MetadataDetailsPtr = nullptr;
		bool bWriteVtxDataFacadeWithCompile = false;

	public:
		const FPCGExGraphBuilderDetails* OutputDetails = nullptr;

		using CompilationEndCallback = std::function<void(const TSharedRef<FGraphBuilder>& InBuilder, const bool bSuccess)>;
		CompilationEndCallback OnCompilationEndCallback;

		SubGraphPostProcessCallback OnSubGraphPostProcess;

		uint32 PairId;
		FString PairIdStr;

		TSharedPtr<FGraph> Graph;

		TSharedRef<PCGExData::FFacade> NodeDataFacade;
		TSharedPtr<PCGEx::FIndexLookup> NodeIndexLookup;

		TSharedPtr<PCGExData::FPointIOCollection> EdgesIO;
		const TArray<TSharedRef<PCGExData::FFacade>>* SourceEdgeFacades = nullptr;

		TSharedPtr<TArray<int32>> OutputNodeIndices;

		bool bCompiledSuccessfully = false;

		FGraphBuilder(
			const TSharedRef<PCGExData::FFacade>& InNodeDataFacade,
			const FPCGExGraphBuilderDetails* InDetails,
			const int32 NumEdgeReserve = 6)
			: OutputDetails(InDetails),
			  NodeDataFacade(InNodeDataFacade)
		{
			PCGEX_LOG_CTR(FGraphBuilder)

			PairId = NodeDataFacade->Source->GetOutIn()->GetUniqueID();
			NodeDataFacade->Source->Tags->Add(TagStr_ClusterPair, PairId, PairIdStr);

			const int32 NumNodes = NodeDataFacade->Source->GetOutInNum();

			Graph = MakeShared<FGraph>(NumNodes, NumEdgeReserve);
			Graph->bBuildClusters = InDetails->bBuildAndCacheClusters;
			Graph->bWriteEdgePosition = OutputDetails->bWriteEdgePosition;
			Graph->EdgePosition = OutputDetails->EdgePosition;
			Graph->bRefreshEdgeSeed = OutputDetails->bRefreshEdgeSeed;

			EdgesIO = MakeShared<PCGExData::FPointIOCollection>(NodeDataFacade->Source->GetContext());
			EdgesIO->OutputPin = OutputEdgesLabel;
		}

		const FGraphMetadataDetails* GetMetadataDetails() const { return MetadataDetailsPtr; }

		void CompileAsync(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager, const bool bWriteNodeFacade, const FGraphMetadataDetails* MetadataDetails = nullptr);
		void Compile(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager, const bool bWriteNodeFacade, const FGraphMetadataDetails* MetadataDetails = nullptr);

		void StageEdgesOutputs() const;

		~FGraphBuilder()
		{
			PCGEX_LOG_DTR(FGraphBuilder)
		}
	};

	static bool BuildEndpointsLookup(
		const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		TMap<uint32, int32>& OutIndices,
		TArray<int32>& OutAdjacency)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExGraph::BuildLookupTable);

		PCGEx::InitArray(OutAdjacency, InPointIO->GetNum());
		OutIndices.Empty();

		const TUniquePtr<PCGExData::TBuffer<int64>> IndexBuffer = MakeUnique<PCGExData::TBuffer<int64>>(InPointIO.ToSharedRef(), Tag_VtxEndpoint);
		if (!IndexBuffer->PrepareRead()) { return false; }

		const TArray<int64>& Indices = *IndexBuffer->GetInValues().Get();

		OutIndices.Reserve(Indices.Num());
		for (int i = 0; i < Indices.Num(); i++)
		{
			uint32 A;
			uint32 B;
			PCGEx::H64(Indices[i], A, B);

			OutIndices.Add(A, i);
			OutAdjacency[i] = B;
		}

		return true;
	}

#pragma endregion

	static bool IsPointDataVtxReady(const UPCGMetadata* Metadata)
	{
		return Metadata->GetConstTypedAttribute<int64>(Tag_VtxEndpoint) ? true : false;
	}

	static bool IsPointDataEdgeReady(const UPCGMetadata* Metadata)
	{
		return Metadata->GetConstTypedAttribute<int64>(Tag_EdgeEndpoints) ? true : false;
	}

	static bool GetReducedVtxIndices(const TSharedPtr<PCGExData::FPointIO>& InEdges, const TMap<uint32, int32>* NodeIndicesMap, TArray<int32>& OutVtxIndices, int32& OutEdgeNum)
	{
		const TUniquePtr<PCGExData::TBuffer<int64>> EndpointsBuffer = MakeUnique<PCGExData::TBuffer<int64>>(InEdges.ToSharedRef(), Tag_EdgeEndpoints);
		if (!EndpointsBuffer->PrepareRead()) { return false; }

		const TArray<int64>& Endpoints = *EndpointsBuffer->GetInValues().Get();

		OutEdgeNum = Endpoints.Num();

		OutVtxIndices.Empty();

		TSet<int32> UniqueVtx;
		UniqueVtx.Reserve(OutEdgeNum * 2);

		for (int i = 0; i < OutEdgeNum; i++)
		{
			uint32 A;
			uint32 B;
			PCGEx::H64(Endpoints[i], A, B);

			const int32* NodeStartPtr = NodeIndicesMap->Find(A);
			const int32* NodeEndPtr = NodeIndicesMap->Find(B);

			if (!NodeStartPtr || !NodeEndPtr || (*NodeStartPtr == *NodeEndPtr)) { continue; }

			UniqueVtx.Add(*NodeStartPtr);
			UniqueVtx.Add(*NodeEndPtr);
		}

		OutVtxIndices.Append(UniqueVtx.Array());
		UniqueVtx.Empty();

		return true;
	}

	static void CleanupVtxData(const TSharedPtr<PCGExData::FPointIO>& PointIO)
	{
		UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;
		PointIO->Tags->Remove(TagStr_ClusterPair);
		Metadata->DeleteAttribute(Tag_VtxEndpoint);
		Metadata->DeleteAttribute(Tag_EdgeEndpoints);
	}
}

namespace PCGExGraphTask
{
#pragma region Graph tasks

	class /*PCGEXTENDEDTOOLKIT_API*/ FWriteSubGraphCluster final : public PCGExMT::FPCGExTask
	{
	public:
		FWriteSubGraphCluster(const TSharedPtr<PCGExGraph::FSubGraph>& InSubGraph)
			: FPCGExTask(),
			  SubGraph(InSubGraph)
		{
		}

		TSharedPtr<PCGExGraph::FSubGraph> SubGraph;
		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const TSharedPtr<PCGExMT::FTaskGroup>& InGroup) override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FCompileGraph final : public PCGExMT::FPCGExTask
	{
	public:
		FCompileGraph(const TSharedPtr<PCGExGraph::FGraphBuilder>& InGraphBuilder,
		              const bool bInWriteNodeFacade,
		              const PCGExGraph::FGraphMetadataDetails* InMetadataDetails = nullptr)
			: FPCGExTask(),
			  Builder(InGraphBuilder),
			  bWriteNodeFacade(bInWriteNodeFacade),
			  MetadataDetails(InMetadataDetails)
		{
		}

		TSharedPtr<PCGExGraph::FGraphBuilder> Builder;
		const bool bWriteNodeFacade = false;
		const PCGExGraph::FGraphMetadataDetails* MetadataDetails = nullptr;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const TSharedPtr<PCGExMT::FTaskGroup>& InGroup) override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FCopyGraphToPoint final : public PCGExMT::FPCGExIndexedTask
	{
	public:
		FCopyGraphToPoint(const int32 InTaskIndex,
		                  const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		                  const TSharedPtr<PCGExGraph::FGraphBuilder>& InGraphBuilder,
		                  const TSharedPtr<PCGExData::FPointIOCollection>& InVtxCollection,
		                  const TSharedPtr<PCGExData::FPointIOCollection>& InEdgeCollection,
		                  FPCGExTransformDetails* InTransformDetails) :
			FPCGExIndexedTask(InTaskIndex),
			PointIO(InPointIO),
			GraphBuilder(InGraphBuilder),
			VtxCollection(InVtxCollection),
			EdgeCollection(InEdgeCollection),
			TransformDetails(InTransformDetails)
		{
		}

		TSharedPtr<PCGExData::FPointIO> PointIO;
		TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder;

		TSharedPtr<PCGExData::FPointIOCollection> VtxCollection;
		TSharedPtr<PCGExData::FPointIOCollection> EdgeCollection;

		FPCGExTransformDetails* TransformDetails = nullptr;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const TSharedPtr<PCGExMT::FTaskGroup>& InGroup) override;
	};

#pragma endregion
}
