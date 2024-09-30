// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Data/PCGExAttributeHelpers.h"
#include "PCGExGlobalSettings.h"
#include "PCGExMT.h"
#include "PCGExEdge.h"
#include "PCGExDetails.h"
#include "Data/PCGExData.h"
#include "PCGExGraph.generated.h"

namespace PCGExGraph
{
	struct FSubGraph;
}

namespace PCGExCluster
{
	struct FCluster;
}

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Graph Value Source"))
enum class EPCGExGraphValueSource : uint8
{
	Vtx  = 0 UMETA(DisplayName = "Point", Tooltip="Value is fetched from the point being evaluated."),
	Edge = 1 UMETA(DisplayName = "Edge", Tooltip="Value is fetched from the edge connecting to the point being evaluated."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Intersection Type"))
enum class EPCGExIntersectionType : uint8
{
	Unknown   = 0 UMETA(DisplayName = "Unknown", ToolTip="Unknown"),
	PointEdge = 1 UMETA(DisplayName = "Point/Edge", ToolTip="Point/Edge Intersection."),
	EdgeEdge  = 2 UMETA(DisplayName = "Edge/Edge", ToolTip="Edge/Edge Intersection."),
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

	/** Expands the cluster data. Takes more space in memory but can be a very effective improvement depending on the operations you're doing on the cluster. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable, EditCondition="bBuildAndCacheClusters"))
	bool bExpandClusters = GetDefault<UPCGExGlobalSettings>()->bDefaultCacheExpandedClusters;

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

	PCGEX_ASYNC_STATE(State_ReadyForNextGraph)
	PCGEX_ASYNC_STATE(State_ProcessingGraph)
	PCGEX_ASYNC_STATE(State_PreparingCompound)
	PCGEX_ASYNC_STATE(State_ProcessingCompound)

	PCGEX_ASYNC_STATE(State_CachingGraphIndices)
	PCGEX_ASYNC_STATE(State_SwappingGraphIndices)

	PCGEX_ASYNC_STATE(State_FindingEdgeTypes)

	PCGEX_ASYNC_STATE(State_BuildCustomGraph)
	PCGEX_ASYNC_STATE(State_FindingCrossings)
	PCGEX_ASYNC_STATE(State_WritingClusters)
	PCGEX_ASYNC_STATE(State_WaitingOnWritingClusters)
	PCGEX_ASYNC_STATE(State_ReadyToCompile)
	PCGEX_ASYNC_STATE(State_Compiling)

	PCGEX_ASYNC_STATE(State_ProcessingPointEdgeIntersections)
	PCGEX_ASYNC_STATE(State_InsertingPointEdgeIntersections)

	PCGEX_ASYNC_STATE(State_FindingEdgeEdgeIntersections)
	PCGEX_ASYNC_STATE(State_InsertingEdgeEdgeIntersections)

	PCGEX_ASYNC_STATE(State_PromotingEdges)
	PCGEX_ASYNC_STATE(State_UpdatingCompoundCenters)

	PCGEX_ASYNC_STATE(State_MergingPointCompounds)
	PCGEX_ASYNC_STATE(State_MergingEdgeCompounds)
	PCGEX_ASYNC_STATE(State_BlendingPointEdgeCrossings)
	PCGEX_ASYNC_STATE(State_ProcessingEdgeEdgeIntersections)

	PCGEX_ASYNC_STATE(State_WritingMainState)
	PCGEX_ASYNC_STATE(State_WritingStatesAttributes)
	PCGEX_ASYNC_STATE(State_WritingIndividualStates)

	PCGEX_ASYNC_STATE(State_ProcessingHeuristics)
	PCGEX_ASYNC_STATE(State_ProcessingHeuristicModifiers)
	PCGEX_ASYNC_STATE(State_Pathfinding)
	PCGEX_ASYNC_STATE(State_WaitingPathfinding)

	class FGraph;

#pragma region Graph

	struct /*PCGEXTENDEDTOOLKIT_API*/ FGraphMetadataDetails
	{
		bool bWriteCompounded = false;
		FName CompoundedAttributeName = "bCompounded";

		bool bWriteCompoundSize = false;
		FName CompoundSizeAttributeName = "CompoundSize";

		bool bWriteCrossing = false;
		FName CrossingAttributeName = "bCrossing";

		bool bWriteIntersector = false;
		FName IntersectorAttributeName = "bIntersector";

		bool bFlagCrossing = false;
		FName FlagA = NAME_None;
		FName FlagB = NAME_None;

		void Grab(const FPCGContext* Context, const FPCGExPointPointIntersectionDetails& InDetails)
		{
			bWriteCompounded = InDetails.bWriteCompounded;
			CompoundedAttributeName = InDetails.CompoundedAttributeName;
			PCGEX_SOFT_VALIDATE_NAME(bWriteCompounded, CompoundedAttributeName, Context)

			bWriteCompoundSize = InDetails.bWriteCompoundSize;
			CompoundSizeAttributeName = InDetails.CompoundSizeAttributeName;
			PCGEX_SOFT_VALIDATE_NAME(bWriteCompoundSize, CompoundSizeAttributeName, Context)
		}

		void Grab(const FPCGContext* Context, const FPCGExEdgeEdgeIntersectionDetails& InDetails)
		{
			bWriteCrossing = InDetails.bWriteCrossing;
			CrossingAttributeName = InDetails.CrossingAttributeName;
			bFlagCrossing = InDetails.bFlagCrossing;
			PCGEX_SOFT_VALIDATE_NAME(bFlagCrossing, FlagA, Context)
			PCGEX_SOFT_VALIDATE_NAME(bFlagCrossing, FlagB, Context)
		}

		void Grab(const FPCGContext* Context, const FPCGExPointEdgeIntersectionDetails& InDetails)
		{
			bWriteIntersector = InDetails.bWriteIntersector;
			IntersectorAttributeName = InDetails.IntersectorAttributeName;
			PCGEX_SOFT_VALIDATE_NAME(bWriteIntersector, IntersectorAttributeName, Context)
		}
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FGraphNodeMetadata
	{
		EPCGExIntersectionType Type = EPCGExIntersectionType::PointEdge;
		bool bCompounded = false; // Represents multiple nodes
		int32 NodeIndex;
		int32 CompoundSize = 0; // Fuse size

		explicit FGraphNodeMetadata(const int32 InNodeIndex)
			: NodeIndex(InNodeIndex)
		{
		}

		bool IsIntersector() const { return Type == EPCGExIntersectionType::PointEdge; }
		bool IsCrossing() const { return Type == EPCGExIntersectionType::EdgeEdge; }

		static FGraphNodeMetadata* GetOrCreate(const int32 NodeIndex, TMap<int32, TUniquePtr<FGraphNodeMetadata>>& InMetadata)
		{
			if (const TUniquePtr<FGraphNodeMetadata>* MetadataPtr = InMetadata.Find(NodeIndex)) { return MetadataPtr->Get(); }
			return InMetadata.Add(NodeIndex, MakeUnique<FGraphNodeMetadata>(NodeIndex)).Get();
		}
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FGraphEdgeMetadata
	{
		int32 EdgeIndex;
		int32 ParentIndex;
		EPCGExIntersectionType Type = EPCGExIntersectionType::Unknown;

		explicit FGraphEdgeMetadata(const int32 InEdgeIndex, const int32 InParentIndex)
			: EdgeIndex(InEdgeIndex), ParentIndex(InParentIndex)
		{
		}

		FORCEINLINE static FGraphEdgeMetadata* GetOrCreate(const int32 EdgeIndex, const int32 ParentIndex, TMap<int32, TUniquePtr<FGraphEdgeMetadata>>& InMetadata)
		{
			if (const TUniquePtr<FGraphEdgeMetadata>* MetadataPtr = InMetadata.Find(EdgeIndex)) { return MetadataPtr->Get(); }
			return InMetadata.Add(EdgeIndex, MakeUnique<FGraphEdgeMetadata>(EdgeIndex, ParentIndex)).Get();
		}

		FORCEINLINE static int32 GetRootIndex(const int32 EdgeIndex, TMap<int32, TUniquePtr<FGraphEdgeMetadata>>& InMetadata)
		{
			int32 ParentIndex = -1;
			const TUniquePtr<FGraphEdgeMetadata>* Parent = InMetadata.Find(EdgeIndex);
			while (Parent)
			{
				ParentIndex = (*Parent)->EdgeIndex;
				Parent = InMetadata.Find(EdgeIndex);
			}

			return ParentIndex == -1 ? EdgeIndex : ParentIndex;
		}
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FNode
	{
		FNode()
		{
		}

		FNode(const int32 InNodeIndex, const int32 InPointIndex):
			NodeIndex(InNodeIndex), PointIndex(InPointIndex)
		{
			Adjacency.Empty();
		}

		bool bValid = true;

		int32 NodeIndex = -1;  // Index in the context of the list that helds the node
		int32 PointIndex = -1; // Index in the context of the UPCGPointData that helds the vtx
		int32 NumExportedEdges = 0;

		TArray<uint64> Adjacency;

		FORCEINLINE void SetAdjacency(const TSet<uint64>& InAdjacency) { Adjacency = InAdjacency.Array(); }
		FORCEINLINE void Add(const int32 EdgeIndex) { Adjacency.AddUnique(EdgeIndex); }

		~FNode() = default;
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FSubGraph
	{
		int64 Id = -1;
		FGraph* ParentGraph = nullptr;
		TSet<int32> Nodes;
		TSet<int32> Edges; //TODO : Test for TArray
		TSet<int32> EdgesInIOIndices;
		TSharedPtr<PCGExData::FFacade> VtxDataFacade;
		TSharedPtr<PCGExData::FFacade> EdgesDataFacade;
		TArray<FIndexedEdge> FlattenedEdges;
		int64 UID = 0;

		FSubGraph()
		{
			PCGEX_LOG_CTR(FSubGraph)
		}

		~FSubGraph()
		{
			PCGEX_LOG_DTR(FSubGraph)
		}

		FORCEINLINE void Add(const FIndexedEdge& Edge, FGraph* InGraph)
		{
			Nodes.Add(Edge.Start);
			Nodes.Add(Edge.End);
			Edges.Add(Edge.EdgeIndex);
			if (Edge.IOIndex >= 0) { EdgesInIOIndices.Add(Edge.IOIndex); }
		}

		void Invalidate(FGraph* InGraph);
		TSharedPtr<PCGExCluster::FCluster> CreateCluster(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) const;
		int32 GetFirstInIOIndex();
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FGraph
	{
		mutable FRWLock GraphLock;
		const int32 NumEdgesReserve;

	public:
		bool bBuildClusters = false;
		bool bExpandClusters = false;

		TArray<FNode> Nodes;
		TMap<int32, TUniquePtr<FGraphNodeMetadata>> NodeMetadata;
		TMap<int32, TUniquePtr<FGraphEdgeMetadata>> EdgeMetadata;

		TArray<FIndexedEdge> Edges;

		TSet<uint64> UniqueEdges;

		TArray<TSharedPtr<FSubGraph>> SubGraphs;

		bool bWriteEdgePosition = true;
		double EdgePosition = 0.5;

		bool bRefreshEdgeSeed = false;

		explicit FGraph(const int32 InNumNodes, const int32 InNumEdgesReserve = 10)
			: NumEdgesReserve(InNumEdgesReserve)
		{
			PCGEX_LOG_CTR(FGraph)

			PCGEx::InitArray(Nodes, InNumNodes);

			for (int i = 0; i < InNumNodes; ++i)
			{
				FNode& Node = Nodes[i];
				Node.NodeIndex = Node.PointIndex = i;
				Node.Adjacency.Reserve(NumEdgesReserve);
			}
		}

		void ReserveForEdges(const int32 UpcomingAdditionCount);

		bool InsertEdgeUnsafe(int32 A, int32 B, FIndexedEdge& OutEdge, int32 IOIndex);
		bool InsertEdge(const int32 A, const int32 B, FIndexedEdge& OutEdge, const int32 IOIndex = -1);

		bool InsertEdgeUnsafe(const FIndexedEdge& Edge);
		bool InsertEdge(const FIndexedEdge& Edge);

		void InsertEdgesUnsafe(const TSet<uint64>& InEdges, int32 InIOIndex);
		void InsertEdges(const TSet<uint64>& InEdges, int32 InIOIndex);

		void InsertEdges(const TArray<uint64>& InEdges, int32 InIOIndex);
		int32 InsertEdges(const TArray<FIndexedEdge>& InEdges);

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
		FGraphMetadataDetails* MetadataDetailsPtr = nullptr;
		bool bWriteVtxDataFacadeWithCompile = false;

	public:
		const FPCGExGraphBuilderDetails* OutputDetails = nullptr;

		using CompilationEndCallback = std::function<void(const TSharedRef<FGraphBuilder>& InBuilder, const bool bSuccess)>;
		CompilationEndCallback OnCompilationEndCallback;

		int64 PairId;
		FString PairIdStr;

		TSharedPtr<FGraph> Graph;

		TSharedPtr<PCGExData::FFacade> NodeDataFacade;

		TSharedPtr<PCGExData::FPointIOCollection> EdgesIO;
		TSharedPtr<PCGExData::FPointIOCollection> SourceEdgesIO;

		bool bCompiledSuccessfully = false;

		FGraphBuilder(const TSharedPtr<PCGExData::FFacade>& InNodeDataFacade, const FPCGExGraphBuilderDetails* InDetails, const int32 NumEdgeReserve = 6, const TSharedPtr<PCGExData::FPointIOCollection>& InSourceEdges = nullptr)
			: OutputDetails(InDetails), NodeDataFacade(InNodeDataFacade), SourceEdgesIO(InSourceEdges)
		{
			PCGEX_LOG_CTR(FGraphBuilder)

			PairId = NodeDataFacade->Source->GetOutIn()->UID;
			NodeDataFacade->Source->Tags->Add(TagStr_ClusterPair, PairId, PairIdStr);

			const int32 NumNodes = NodeDataFacade->Source->GetOutInNum();

			Graph = MakeShared<FGraph>(NumNodes, NumEdgeReserve);
			Graph->bBuildClusters = InDetails->bBuildAndCacheClusters;
			Graph->bExpandClusters = InDetails->bExpandClusters;
			Graph->bWriteEdgePosition = OutputDetails->bWriteEdgePosition;
			Graph->EdgePosition = OutputDetails->EdgePosition;
			Graph->bRefreshEdgeSeed = OutputDetails->bRefreshEdgeSeed;

			EdgesIO = MakeShared<PCGExData::FPointIOCollection>(NodeDataFacade->Source->GetContext());
			EdgesIO->DefaultOutputLabel = OutputEdgesLabel;
		}

		void CompileAsync(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager, const bool bWriteNodeFacade, FGraphMetadataDetails* MetadataDetails = nullptr);
		void Compile(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager, const bool bWriteNodeFacade, FGraphMetadataDetails* MetadataDetails = nullptr);

		void OutputEdgesToContext() const;

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

		const TUniquePtr<PCGEx::TAttributeReader<int64>> IndexReader = MakeUnique<PCGEx::TAttributeReader<int64>>(Tag_VtxEndpoint);
		if (!IndexReader->Bind(InPointIO)) { return false; }

		OutIndices.Reserve(IndexReader->Values.Num());
		for (int i = 0; i < IndexReader->Values.Num(); ++i)
		{
			uint32 A;
			uint32 B;
			PCGEx::H64(IndexReader->Values[i], A, B);

			OutIndices.Add(A, i);
			OutAdjacency[i] = B;
		}

		return true;
	}

#pragma endregion

	static bool IsPointDataVtxReady(const UPCGMetadata* Metadata)
	{
		constexpr int16 I64 = static_cast<uint16>(EPCGMetadataTypes::Integer64);
		//constexpr int16 I32 = static_cast<uint16>(EPCGMetadataTypes::Integer32);

		const FPCGMetadataAttributeBase* EndpointAttribute = Metadata->GetConstAttribute(Tag_VtxEndpoint);
		if (!EndpointAttribute || EndpointAttribute->GetTypeId() != I64) { return false; }

		const FPCGMetadataAttributeBase* ClusterIdAttribute = Metadata->GetConstAttribute(Tag_ClusterId);
		if (!ClusterIdAttribute || ClusterIdAttribute->GetTypeId() != I64) { return false; }

		return true;
	}

	static bool IsPointDataEdgeReady(const UPCGMetadata* Metadata)
	{
		constexpr int16 I64 = static_cast<uint16>(EPCGMetadataTypes::Integer64);
		constexpr int16 I32 = static_cast<uint16>(EPCGMetadataTypes::Integer32);

		const FPCGMetadataAttributeBase* EndpointAttribute = Metadata->GetConstAttribute(Tag_EdgeEndpoints);
		if (!EndpointAttribute || EndpointAttribute->GetTypeId() != I64) { return false; }

		const FPCGMetadataAttributeBase* ClusterIdAttribute = Metadata->GetConstAttribute(Tag_ClusterId);
		if (!ClusterIdAttribute || ClusterIdAttribute->GetTypeId() != I64) { return false; }

		return true;
	}

	static bool GetReducedVtxIndices(const TSharedPtr<PCGExData::FPointIO>& InEdges, const TMap<uint32, int32>* NodeIndicesMap, TArray<int32>& OutVtxIndices, int32& OutEdgeNum)
	{
		const TUniquePtr<PCGEx::TAttributeReader<int64>> EndpointsReader = MakeUnique<PCGEx::TAttributeReader<int64>>(Tag_EdgeEndpoints);

		if (!EndpointsReader->Bind(InEdges)) { return false; }

		OutEdgeNum = EndpointsReader->Values.Num();

		OutVtxIndices.Empty();

		TSet<int32> UniqueVtx;
		UniqueVtx.Reserve(OutEdgeNum * 2);

		for (int i = 0; i < OutEdgeNum; ++i)
		{
			uint32 A;
			uint32 B;
			PCGEx::H64(EndpointsReader->Values[i], A, B);

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

	static void WriteSubGraphEdges(
		const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager,
		const TSharedPtr<PCGExGraph::FSubGraph>& SubGraph,
		const PCGExGraph::FGraphMetadataDetails* MetadataDetails);

	class /*PCGEXTENDEDTOOLKIT_API*/ FWriteSubGraphCluster final : public PCGExMT::FPCGExTask
	{
	public:
		FWriteSubGraphCluster(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		                      const TSharedPtr<PCGExGraph::FSubGraph>& InSubGraph)
			: FPCGExTask(InPointIO),
			  SubGraph(InSubGraph)
		{
		}

		const TSharedPtr<PCGExGraph::FSubGraph> SubGraph;
		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FCompileGraph final : public PCGExMT::FPCGExTask
	{
	public:
		FCompileGraph(
			const TSharedPtr<PCGExData::FPointIO>& InPointIO,
			const TSharedPtr<PCGExGraph::FGraphBuilder>& InGraphBuilder,
			const bool bInWriteNodeFacade,
			PCGExGraph::FGraphMetadataDetails* InMetadataDetails = nullptr)
			: FPCGExTask(InPointIO),
			  Builder(InGraphBuilder),
			  bWriteNodeFacade(bInWriteNodeFacade),
			  MetadataDetails(InMetadataDetails)
		{
		}

		const TSharedPtr<PCGExGraph::FGraphBuilder> Builder;
		const bool bWriteNodeFacade = false;
		PCGExGraph::FGraphMetadataDetails* MetadataDetails = nullptr;

		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FCopyGraphToPoint final : public PCGExMT::FPCGExTask
	{
	public:
		FCopyGraphToPoint(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		                  const TSharedPtr<PCGExGraph::FGraphBuilder>& InGraphBuilder,
		                  const TSharedPtr<PCGExData::FPointIOCollection>& InVtxCollection,
		                  const TSharedPtr<PCGExData::FPointIOCollection>& InEdgeCollection,
		                  FPCGExTransformDetails* InTransformDetails) :
			FPCGExTask(InPointIO),
			GraphBuilder(InGraphBuilder),
			VtxCollection(InVtxCollection),
			EdgeCollection(InEdgeCollection),
			TransformDetails(InTransformDetails)
		{
		}

		TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder;

		TSharedPtr<PCGExData::FPointIOCollection> VtxCollection;
		TSharedPtr<PCGExData::FPointIOCollection> EdgeCollection;

		FPCGExTransformDetails* TransformDetails = nullptr;

		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};

#pragma endregion
}
