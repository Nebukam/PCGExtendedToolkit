// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Data/PCGExAttributeHelpers.h"
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
enum class EPCGExClusterElement : uint8
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

UENUM()
enum class EPCGExBasicEdgeRadius : uint8
{
	Average = 0 UMETA(DisplayName = "Average", ToolTip="Edge radius is the average of each endpoint' bounds radii"),
	Lerp    = 1 UMETA(DisplayName = "Lerp", ToolTip="Edge radius is the edge lerp position between endpoint' bounds radii"),
	Min     = 2 UMETA(DisplayName = "Min", ToolTip="Edge radius is the smallest endpoint' bounds radius"),
	Max     = 3 UMETA(DisplayName = "Max", ToolTip="Edge radius is the largest endpoint' bounds radius"),
	Fixed   = 4 UMETA(DisplayName = "Fixed", ToolTip="Edge radius is a fixed size"),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExBasicEdgeSolidificationDetails
{
	GENERATED_BODY()

	FPCGExBasicEdgeSolidificationDetails() = default;

	/** Align the edge point to the edge direction over the selected axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExMinimalAxis SolidificationAxis = EPCGExMinimalAxis::None;

	/** Pick how edge radius should be calculated in regard to its endpoints */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_NotOverridable))
	EPCGExBasicEdgeRadius RadiusType = EPCGExBasicEdgeRadius::Lerp;

	/** Fixed edge radius */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_NotOverridable, EditCondition="RadiusType==EPCGExBasicEdgeRadius::Fixed", EditConditionHides))
	double RadiusConstant = 5;

	/** Scale the computed radius by a factor */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_NotOverridable, EditCondition="RadiusType!=EPCGExBasicEdgeRadius::Fixed", EditConditionHides))
	double RadiusScale = 1;

	void Mutate(PCGExData::FMutablePoint& InEdgePoint, const PCGExData::FConstPoint& InStart, const PCGExData::FConstPoint& InEnd, const double InLerp) const;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExGraphBuilderDetails
{
	GENERATED_BODY()

	explicit FPCGExGraphBuilderDetails(const EPCGExMinimalAxis InDefaultSolidificationAxis = EPCGExMinimalAxis::None);

	/** Don't output Clusters if they have less points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteEdgePosition = true;

	/** Edge position interpolation between start and end point positions. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable, EditCondition="bWriteEdgePosition"))
	double EdgePosition = 0.5;

	/** If enabled, does some basic solidification of the edges over the X axis as a default. If you need full control, use the Edge Properties node. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_NotOverridable, DisplayName=" └─ Solidification", EditCondition="bWriteEdgePosition", HideEditConditionToggle))
	FPCGExBasicEdgeSolidificationDetails BasicEdgeSolidification;

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
	EPCGExOptionState BuildAndCacheClusters = EPCGExOptionState::Default;

	/**  */
	UPROPERTY(BlueprintReadWrite, Category = "Settings|Extra Data", EditAnywhere, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputEdgeLength = false;

	/** Whether to output edge length to a 'double' attribute. */
	UPROPERTY(BlueprintReadWrite, Category = "Settings|Extra Data", EditAnywhere, meta = (PCG_Overridable, EditCondition="bOutputEdgeLength"))
	FName EdgeLengthName = FName("EdgeLength");

	bool WantsClusters() const;

	bool IsValid(const TSharedPtr<PCGExGraph::FSubGraph>& InSubgraph) const;
};

namespace PCGExGraph
{

	using NodeLinks = TArray<FLink, TInlineAllocator<8>>;
	
	using FGraphCompilationEndCallback = std::function<void(const TSharedRef<FGraphBuilder>& InBuilder, const bool bSuccess)>;

	const FName SourceProbesLabel = TEXT("Probes");
	const FName OutputProbeLabel = TEXT("Probe");

	const FName SourceFilterGenerators = TEXT("Generator Filters");
	const FName SourceFilterConnectables = TEXT("Connectable Filters");

	const FName SourceGraphsLabel = TEXT("In");
	const FName OutputGraphsLabel = TEXT("Out");

	const FName SourceVerticesLabel = TEXT("Vtx");
	const FName OutputVerticesLabel = TEXT("Vtx");

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

	bool BuildIndexedEdges(
		const TSharedPtr<PCGExData::FPointIO>& EdgeIO,
		const TMap<uint32, int32>& EndpointsLookup,
		TArray<FEdge>& OutEdges,
		const bool bStopOnError = false);

#pragma endregion

#pragma region Graph

	struct PCGEXTENDEDTOOLKIT_API FGraphMetadataDetails
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
		EPCGExIntersectionType Type = EPCGExIntersectionType::Unknown;
		int32 NodeIndex;
		int32 UnionSize = 0; // Fuse size
		bool IsUnion() const;

		explicit FGraphNodeMetadata(const int32 InNodeIndex);

		bool IsIntersector() const;
		bool IsCrossing() const;
	};

	struct PCGEXTENDEDTOOLKIT_API FGraphEdgeMetadata
	{
		int32 EdgeIndex;
		int32 ParentIndex;
		int32 RootIndex;
		EPCGExIntersectionType Type = EPCGExIntersectionType::Unknown;

		int32 UnionSize = 0; // Fuse size
		bool IsUnion() const;

		explicit FGraphEdgeMetadata(const int32 InEdgeIndex, const FGraphEdgeMetadata* Parent);
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

	class PCGEXTENDEDTOOLKIT_API FSubGraph : public TSharedFromThis<FSubGraph>
	{
	public:
		TWeakPtr<FGraph> WeakParentGraph;
		TSet<int32> Nodes;
		TSet<int32> Edges;
		TSet<int32> EdgesInIOIndices;
		TSharedPtr<PCGExData::FFacade> VtxDataFacade;
		TSharedPtr<PCGExData::FFacade> EdgesDataFacade;
		TArray<FEdge> FlattenedEdges;
		int32 UID = 0;
		SubGraphPostProcessCallback OnSubGraphPostProcess;


		FSubGraph() = default;

		~FSubGraph() = default;

		void Add(const FEdge& Edge, FGraph* InGraph);

		void Invalidate(FGraph* InGraph);
		void BuildCluster(const TSharedRef<PCGExCluster::FCluster>& InCluster);
		int32 GetFirstInIOIndex();

		void Compile(
			const TWeakPtr<PCGExMT::FAsyncMultiHandle>& InParentHandle,
			const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager,
			const TSharedPtr<FGraphBuilder>& InBuilder);

	protected:
		TWeakPtr<PCGExMT::FTaskManager> WeakAsyncManager;
		TWeakPtr<FGraphBuilder> WeakBuilder;

		const FGraphMetadataDetails* MetadataDetails = nullptr;

		TSharedPtr<PCGExDataBlending::FUnionBlender> UnionBlender;
		TSharedPtr<PCGExDetails::FDistances> Distances;

		// Edge metadata
#define PCGEX_FOREACH_EDGE_METADATA(MACRO)\
MACRO(IsEdgeUnion, bool, false, IsUnion()) \
MACRO(EdgeUnionSize, int32, 0, UnionSize)

#define PCGEX_EDGE_METADATA_DECL(_NAME, _TYPE, _DEFAULT, _ACCESSOR) TSharedPtr<PCGExData::TBuffer<_TYPE>> _NAME##Buffer;
		PCGEX_FOREACH_EDGE_METADATA(PCGEX_EDGE_METADATA_DECL)
#undef PCGEX_EDGE_METADATA_DECL

#undef PCGEX_FOREACH_EDGE_METADATA

		// Extra edge data
		TSharedPtr<PCGExData::TBuffer<double>> EdgeLength;

		void CompileRange(const PCGExMT::FScope& Scope);
		void CompilationComplete();
	};

	class PCGEXTENDEDTOOLKIT_API FGraph : public TSharedFromThis<FGraph>
	{
		mutable FRWLock GraphLock;
		mutable FRWLock EdgeMetadataLock;
		mutable FRWLock NodeMetadataLock;

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

		FEdge* FindEdge_Unsafe(const uint64 Hash);
		FEdge* FindEdge_Unsafe(const int32 A, const int32 B);
		FEdge* FindEdge(const uint64 Hash);
		FEdge* FindEdge(const int32 A, const int32 B);

		FGraphEdgeMetadata& GetOrCreateEdgeMetadata_Unsafe(const int32 EdgeIndex, const FGraphEdgeMetadata* Parent = nullptr);
		FGraphEdgeMetadata& GetOrCreateEdgeMetadata(const int32 EdgeIndex, const FGraphEdgeMetadata* Parent = nullptr);
		FGraphNodeMetadata& GetOrCreateNodeMetadata_Unsafe(const int32 NodeIndex);
		FGraphNodeMetadata& GetOrCreateNodeMetadata(const int32 NodeIndex);


		void AddNodeAndEdgeMetadata_Unsafe(
			const int32 InNodeIndex,
			const int32 InEdgeIndex,
			const FGraphEdgeMetadata* InParentMetadata,
			const EPCGExIntersectionType InType);


		void AddNodeAndEdgeMetadata(
			const int32 InNodeIndex,
			const int32 InEdgeIndex,
			const FGraphEdgeMetadata* InParentMetadata,
			const EPCGExIntersectionType InType);


		void AddNodeMetadata_Unsafe(
			const int32 InNodeIndex,
			const FGraphEdgeMetadata* InParentMetadata,
			const EPCGExIntersectionType InType);


		void AddNodeMetadata(
			const int32 InNodeIndex,
			const FGraphEdgeMetadata* InParentMetadata,
			const EPCGExIntersectionType InType);


		void AddEdgeMetadata_Unsafe(
			const int32 InEdgeIndex,
			const FGraphEdgeMetadata* InParentMetadata,
			const EPCGExIntersectionType InType);


		void AddEdgeMetadata(
			const int32 InEdgeIndex,
			const FGraphEdgeMetadata* InParentMetadata,
			const EPCGExIntersectionType InType);


		FGraphNodeMetadata* FindNodeMetadata_Unsafe(const int32 NodeIndex) { return NodeMetadata.Find(NodeIndex); }
		FGraphNodeMetadata* FindNodeMetadata(const int32 NodeIndex);
		FGraphEdgeMetadata* FindEdgeMetadata_Unsafe(const int32 EdgeIndex) { return EdgeMetadata.Find(EdgeIndex); }
		FGraphEdgeMetadata* FindEdgeMetadata(const int32 EdgeIndex);
		FGraphEdgeMetadata* FindRootEdgeMetadata_Unsafe(const int32 EdgeIndex);
		FGraphEdgeMetadata* FindRootEdgeMetadata(const int32 EdgeIndex);

		TArrayView<FNode> AddNodes(const int32 NumNewNodes);

		void BuildSubGraphs(const FPCGExGraphBuilderDetails& Limits);

		~FGraph() = default;

		void GetConnectedNodes(int32 FromIndex, TArray<int32>& OutIndices, int32 SearchDepth) const;
	};

	class PCGEXTENDEDTOOLKIT_API FGraphBuilder : public TSharedFromThis<FGraphBuilder>
	{
	protected:
		TSharedPtr<PCGExMT::FTaskManager> AsyncManager;
		const FGraphMetadataDetails* MetadataDetailsPtr = nullptr;
		bool bWriteVtxDataFacadeWithCompile = false;
		bool bCompiling = false;

	public:
		const FPCGExGraphBuilderDetails* OutputDetails = nullptr;

		FGraphCompilationEndCallback OnCompilationEndCallback;

		SubGraphPostProcessCallback OnSubGraphPostProcess;

		PCGExTags::IDType PairId;
		TSharedPtr<FGraph> Graph;

		TSharedRef<PCGExData::FFacade> NodeDataFacade;
		TSharedPtr<PCGEx::FIndexLookup> NodeIndexLookup;

		TSharedPtr<PCGExData::FPointIOCollection> EdgesIO;
		const TArray<TSharedRef<PCGExData::FFacade>>* SourceEdgeFacades = nullptr;

		TSharedPtr<TArray<int32>> OutputNodeIndices;
		TSharedPtr<TArray<int32>> OutputPointIndices;

		bool bInheritNodeData = true;
		bool bCompiledSuccessfully = false;

		FGraphBuilder(
			const TSharedRef<PCGExData::FFacade>& InNodeDataFacade,
			const FPCGExGraphBuilderDetails* InDetails,
			const int32 NumEdgeReserve = 6);

		const FGraphMetadataDetails* GetMetadataDetails() const { return MetadataDetailsPtr; }

		void CompileAsync(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager, const bool bWriteNodeFacade, const FGraphMetadataDetails* MetadataDetails = nullptr);
		void Compile(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager, const bool bWriteNodeFacade, const FGraphMetadataDetails* MetadataDetails = nullptr);

		void StageEdgesOutputs() const;

		~FGraphBuilder() = default;
	};

	bool BuildEndpointsLookup(
		const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		TMap<uint32, int32>& OutIndices,
		TArray<int32>& OutAdjacency);

#pragma endregion

	bool IsPointDataVtxReady(const UPCGMetadata* Metadata);
	bool IsPointDataEdgeReady(const UPCGMetadata* Metadata);
	void CleanupVtxData(const TSharedPtr<PCGExData::FPointIO>& PointIO);
}

namespace PCGExGraphTask
{
#pragma region Graph tasks

	class FWriteSubGraphCluster final : public PCGExMT::FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FWriteSubGraphCluster)

		FWriteSubGraphCluster(const TSharedPtr<PCGExGraph::FSubGraph>& InSubGraph)
			: FTask(),
			  SubGraph(InSubGraph)
		{
		}

		TSharedPtr<PCGExGraph::FSubGraph> SubGraph;
		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};

	class FCompileGraph final : public PCGExMT::FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FCompileGraph)

		FCompileGraph(const TSharedPtr<PCGExGraph::FGraphBuilder>& InGraphBuilder,
		              const bool bInWriteNodeFacade,
		              const PCGExGraph::FGraphMetadataDetails* InMetadataDetails = nullptr)
			: FTask(),
			  Builder(InGraphBuilder),
			  bWriteNodeFacade(bInWriteNodeFacade),
			  MetadataDetails(InMetadataDetails)
		{
		}

		TSharedPtr<PCGExGraph::FGraphBuilder> Builder;
		const bool bWriteNodeFacade = false;
		const PCGExGraph::FGraphMetadataDetails* MetadataDetails = nullptr;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};

	class FCopyGraphToPoint final : public PCGExMT::FPCGExIndexedTask
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

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};

#pragma endregion
}
