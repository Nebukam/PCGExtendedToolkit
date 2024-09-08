// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Data/PCGExAttributeHelpers.h"
#include "PCGExMT.h"
#include "PCGExEdge.h"
#include "PCGExGlobalSettings.h"
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

namespace PCGExGeo
{
	class FGeoMesh;
}

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Graph Value Source"))
enum class EPCGExGraphValueSource : uint8
{
	Vtx UMETA(DisplayName = "Point", Tooltip="Value is fetched from the point being evaluated."),
	Edge UMETA(DisplayName = "Edge", Tooltip="Value is fetched from the edge connecting to the point being evaluated."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Intersection Type"))
enum class EPCGExIntersectionType : uint8
{
	Unknown UMETA(DisplayName = "Unknown", ToolTip="Unknown"),
	PointEdge UMETA(DisplayName = "Point/Edge", ToolTip="Point/Edge Intersection."),
	EdgeEdge UMETA(DisplayName = "Edge/Edge", ToolTip="Edge/Edge Intersection."),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExGraphBuilderDetails
{
	GENERATED_BODY()

	FPCGExGraphBuilderDetails()
	{
	}

	explicit FPCGExGraphBuilderDetails(const bool PrunePoints)
		: bPruneIsolatedPoints(PrunePoints)
	{
	}

	/** Removes roaming points from the output, and keeps only points that are part of an cluster. */
	//UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable))
	bool bPruneIsolatedPoints = true;

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

	bool IsValid(const PCGExGraph::FSubGraph* InSubgraph) const;
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
		int32 NodeIndex;
		EPCGExIntersectionType Type = EPCGExIntersectionType::PointEdge;
		bool bCompounded = false; // Represents multiple nodes
		int32 CompoundSize = 0;   // Fuse size

		explicit FGraphNodeMetadata(const int32 InNodeIndex)
			: NodeIndex(InNodeIndex)
		{
		}

		bool IsIntersector() const { return Type == EPCGExIntersectionType::PointEdge; }
		bool IsCrossing() const { return Type == EPCGExIntersectionType::EdgeEdge; }

		static FGraphNodeMetadata* GetOrCreate(const int32 NodeIndex, TMap<int32, FGraphNodeMetadata*>& InMetadata)
		{
			if (FGraphNodeMetadata** MetadataPtr = InMetadata.Find(NodeIndex)) { return *MetadataPtr; }

			FGraphNodeMetadata* NewMetadata = new FGraphNodeMetadata(NodeIndex);
			InMetadata.Add(NodeIndex, NewMetadata);
			return NewMetadata;
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

		FORCEINLINE static FGraphEdgeMetadata* GetOrCreate(const int32 EdgeIndex, const int32 ParentIndex, TMap<int32, FGraphEdgeMetadata*>& InMetadata)
		{
			if (FGraphEdgeMetadata** MetadataPtr = InMetadata.Find(EdgeIndex)) { return *MetadataPtr; }

			FGraphEdgeMetadata* NewMetadata = new FGraphEdgeMetadata(EdgeIndex, ParentIndex);
			InMetadata.Add(EdgeIndex, NewMetadata);
			return NewMetadata;
		}

		FORCEINLINE static int32 GetRootIndex(const int32 EdgeIndex, TMap<int32, FGraphEdgeMetadata*>& InMetadata)
		{
			int32 ParentIndex = -1;
			FGraphEdgeMetadata** Parent = InMetadata.Find(EdgeIndex);
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

		~FNode()
		{
			Adjacency.Empty();
		}
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FSubGraph
	{
		int64 Id = -1;
		FGraph* ParentGraph = nullptr;
		TSet<int32> Nodes;
		TSet<int32> Edges; //TODO : Test for TArray
		TSet<int32> EdgesInIOIndices;
		PCGExData::FPointIO* VtxIO = nullptr;
		PCGExData::FPointIO* EdgesIO = nullptr;
		TArray<FIndexedEdge> FlattenedEdges;

		FSubGraph()
		{
		}

		~FSubGraph()
		{
			Nodes.Empty();
			Edges.Empty();
			FlattenedEdges.Empty();
			EdgesInIOIndices.Empty();
			VtxIO = nullptr;
			EdgesIO = nullptr;
		}

		FORCEINLINE void Add(const FIndexedEdge& Edge, FGraph* InGraph)
		{
			Nodes.Add(Edge.Start);
			Nodes.Add(Edge.End);
			Edges.Add(Edge.EdgeIndex);
			if (Edge.IOIndex >= 0) { EdgesInIOIndices.Add(Edge.IOIndex); }
		}

		void Invalidate(FGraph* InGraph);
		PCGExCluster::FCluster* CreateCluster(PCGExMT::FTaskManager* AsyncManager) const;
		int32 GetFirstInIOIndex();
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FGraph
	{
		mutable FRWLock GraphLock;
		const int32 NumEdgesReserve;

	public:
		bool bRequiresConsolidation = false;
		bool bBuildClusters = false;
		bool bExpandClusters = false;

		TArray<FNode> Nodes;
		TMap<int32, FGraphNodeMetadata*> NodeMetadata;
		TMap<int32, FGraphEdgeMetadata*> EdgeMetadata;

		TArray<FIndexedEdge> Edges;

		TSet<uint64> UniqueEdges;

		TArray<FSubGraph*> SubGraphs;

		bool bWriteEdgePosition = true;
		double EdgePosition = 0.5;

		bool bRefreshEdgeSeed = false;

		explicit FGraph(const int32 InNumNodes, const int32 InNumEdgesReserve = 10)
			: NumEdgesReserve(InNumEdgesReserve)
		{
			PCGEX_SET_NUM(Nodes, InNumNodes)

			for (int i = 0; i < InNumNodes; i++)
			{
				FNode& Node = Nodes[i];
				Node.NodeIndex = Node.PointIndex = i;
				Node.Adjacency.Reserve(NumEdgesReserve);
			}
		}

		bool InsertEdge(const int32 A, const int32 B, FIndexedEdge& OutEdge, const int32 IOIndex = -1);
		bool InsertEdge(const FIndexedEdge& Edge);
		void InsertEdges(const TSet<uint64>& InEdges, int32 InIOIndex);
		void InsertEdges(const TArray<uint64>& InEdges, int32 InIOIndex);
		void InsertEdges(const TArray<FUnsignedEdge>& InEdges, int32 InIOIndex);
		void InsertEdges(const TArray<FIndexedEdge>& InEdges);

		void InsertEdgesUnsafe(const TSet<uint64>& InEdges, int32 InIOIndex);

		TArrayView<FNode> AddNodes(const int32 NumNewNodes);

		void BuildSubGraphs(const FPCGExGraphBuilderDetails& Limits);

		void ForEachCluster(TFunction<void(FSubGraph*)>&& Func)
		{
			for (FSubGraph* Cluster : SubGraphs)
			{
				if (Cluster->Nodes.IsEmpty() || Cluster->Edges.IsEmpty()) { continue; }
				Func(Cluster);
			}
		}

		~FGraph()
		{
			PCGEX_DELETE_TMAP(NodeMetadata, int32)
			PCGEX_DELETE_TMAP(EdgeMetadata, int32)

			Nodes.Empty();
			UniqueEdges.Empty();
			Edges.Empty();

			for (const FSubGraph* Cluster : SubGraphs) { delete Cluster; }
			SubGraphs.Empty();
		}

		void GetConnectedNodes(int32 FromIndex, TArray<int32>& OutIndices, int32 SearchDepth) const;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FGraphBuilder
	{
	public:
		const FPCGExGraphBuilderDetails* OutputDetails = nullptr;

		bool bPrunePoints = false;
		int64 PairId;
		FString PairIdStr;

		PCGExData::FPointIO* PointIO = nullptr;

		FGraph* Graph = nullptr;

		PCGExData::FPointIOCollection* EdgesIO = nullptr;
		PCGExData::FPointIOCollection* SourceEdgesIO = nullptr;

		PCGExData::FFacade* VtxDataFacade = nullptr;

		bool bCompiledSuccessfully = false;

		FGraphBuilder(PCGExData::FPointIO* InPointIO, const FPCGExGraphBuilderDetails* InDetails, const int32 NumEdgeReserve = 6, PCGExData::FPointIOCollection* InSourceEdges = nullptr)
			: OutputDetails(InDetails), SourceEdgesIO(InSourceEdges)
		{
			PointIO = InPointIO;
			PairId = PointIO->GetOutIn()->UID;
			PointIO->Tags->Add(TagStr_ClusterPair, PairId, PairIdStr);

			const int32 NumNodes = PointIO->GetOutInNum();
			VtxDataFacade = new PCGExData::FFacade(PointIO);

			Graph = new FGraph(NumNodes, NumEdgeReserve);
			Graph->bBuildClusters = InDetails->bBuildAndCacheClusters;
			Graph->bExpandClusters = InDetails->bExpandClusters;
			Graph->bWriteEdgePosition = OutputDetails->bWriteEdgePosition;
			Graph->EdgePosition = OutputDetails->EdgePosition;
			Graph->bRefreshEdgeSeed = OutputDetails->bRefreshEdgeSeed;

			EdgesIO = new PCGExData::FPointIOCollection(InPointIO->GetContext());
			EdgesIO->DefaultOutputLabel = OutputEdgesLabel;

			bPrunePoints = OutputDetails->bPruneIsolatedPoints;
		}

		void CompileAsync(PCGExMT::FTaskManager* AsyncManager, FGraphMetadataDetails* MetadataDetails = nullptr);
		void Compile(PCGExMT::FTaskManager* AsyncManager, FGraphMetadataDetails* MetadataDetails = nullptr);

		void Write() const;

		~FGraphBuilder()
		{
			PCGEX_DELETE(Graph)
			PCGEX_DELETE(EdgesIO)
			PCGEX_DELETE(VtxDataFacade)
		}
	};

	static bool BuildEndpointsLookup(
		const PCGExData::FPointIO* InPointIO,
		TMap<uint32, int32>& OutIndices,
		TArray<int32>& OutAdjacency)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExGraph::BuildLookupTable);

		PCGEX_SET_NUM_UNINITIALIZED(OutAdjacency, InPointIO->GetNum())
		OutIndices.Empty();

		PCGEx::TAttributeReader<int64>* IndexReader = new PCGEx::TAttributeReader<int64>(Tag_VtxEndpoint);
		if (!IndexReader->Bind(const_cast<PCGExData::FPointIO*>(InPointIO)))
		{
			PCGEX_DELETE(IndexReader)
			return false;
		}

		OutIndices.Reserve(IndexReader->Values.Num());
		for (int i = 0; i < IndexReader->Values.Num(); i++)
		{
			uint32 A;
			uint32 B;
			PCGEx::H64(IndexReader->Values[i], A, B);

			OutIndices.Add(A, i);
			OutAdjacency[i] = B;
		}

		PCGEX_DELETE(IndexReader)
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

	static bool GetReducedVtxIndices(PCGExData::FPointIO* InEdges, const TMap<uint32, int32>* NodeIndicesMap, TArray<int32>& OutVtxIndices, int32& OutEdgeNum)
	{
		PCGEx::TAttributeReader<int64>* EndpointsReader = new PCGEx::TAttributeReader<int64>(Tag_EdgeEndpoints);

		if (!EndpointsReader->Bind(InEdges))
		{
			PCGEX_DELETE(EndpointsReader)
			return false;
		}

		OutEdgeNum = EndpointsReader->Values.Num();

		OutVtxIndices.Empty();

		TSet<int32> UniqueVtx;
		UniqueVtx.Reserve(OutEdgeNum * 2);

		for (int i = 0; i < OutEdgeNum; i++)
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

		PCGEX_DELETE(EndpointsReader)
		return true;
	}

	static void CleanupVtxData(const PCGExData::FPointIO* PointIO)
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
		PCGExMT::FTaskManager* AsyncManager,
		const UPCGPointData* VtxIO,
		PCGExGraph::FSubGraph* SubGraph, const PCGExGraph::FGraphMetadataDetails* MetadataDetails);

	class /*PCGEXTENDEDTOOLKIT_API*/ FWriteSubGraphEdges final : public PCGExMT::FPCGExTask
	{
	public:
		FWriteSubGraphEdges(PCGExData::FPointIO* InPointIO,
		                    PCGExGraph::FSubGraph* InSubGraph,
		                    PCGExGraph::FGraphMetadataDetails* InMetadataDetails = nullptr)
			: FPCGExTask(InPointIO),
			  SubGraph(InSubGraph),
			  MetadataDetails(InMetadataDetails)
		{
		}

		PCGExGraph::FSubGraph* SubGraph = nullptr;
		PCGExGraph::FGraphMetadataDetails* MetadataDetails = nullptr;

		virtual bool ExecuteTask() override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FWriteSmallSubGraphEdges final : public PCGExMT::FPCGExTask
	{
	public:
		FWriteSmallSubGraphEdges(
			PCGExData::FPointIO* InPointIO,
			const TArray<PCGExGraph::FSubGraph*>& InSubGraphs,
			PCGExGraph::FGraphMetadataDetails* InMetadataDetails = nullptr)
			: FPCGExTask(InPointIO),
			  SubGraphs(InSubGraphs),
			  MetadataDetails(InMetadataDetails)
		{
		}

		virtual ~FWriteSmallSubGraphEdges() override
		{
			SubGraphs.Empty();
		}

		TArray<PCGExGraph::FSubGraph*> SubGraphs;

		PCGExGraph::FGraphMetadataDetails* MetadataDetails = nullptr;

		virtual bool ExecuteTask() override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FWriteSubGraphCluster final : public PCGExMT::FPCGExTask
	{
	public:
		FWriteSubGraphCluster(PCGExData::FPointIO* InPointIO,
		                      PCGExGraph::FSubGraph* InSubGraph)
			: FPCGExTask(InPointIO),
			  SubGraph(InSubGraph)
		{
		}

		PCGExGraph::FSubGraph* SubGraph = nullptr;
		virtual bool ExecuteTask() override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FCompileGraph final : public PCGExMT::FPCGExTask
	{
	public:
		FCompileGraph(
			PCGExData::FPointIO* InPointIO,
			PCGExGraph::FGraphBuilder* InGraphBuilder,
			PCGExGraph::FGraphMetadataDetails* InMetadataDetails = nullptr)
			: FPCGExTask(InPointIO),
			  Builder(InGraphBuilder),
			  MetadataDetails(InMetadataDetails)
		{
		}

		PCGExGraph::FGraphBuilder* Builder = nullptr;
		PCGExGraph::FGraphMetadataDetails* MetadataDetails = nullptr;

		virtual bool ExecuteTask() override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FCopyGraphToPoint final : public PCGExMT::FPCGExTask
	{
	public:
		FCopyGraphToPoint(PCGExData::FPointIO* InPointIO,
		                  PCGExGraph::FGraphBuilder* InGraphBuilder,
		                  PCGExData::FPointIOCollection* InVtxCollection,
		                  PCGExData::FPointIOCollection* InEdgeCollection,
		                  FPCGExTransformDetails* InTransformDetails) :
			FPCGExTask(InPointIO),
			GraphBuilder(InGraphBuilder),
			VtxCollection(InVtxCollection),
			EdgeCollection(InEdgeCollection),
			TransformDetails(InTransformDetails)
		{
		}

		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;

		PCGExData::FPointIOCollection* VtxCollection = nullptr;
		PCGExData::FPointIOCollection* EdgeCollection = nullptr;

		FPCGExTransformDetails* TransformDetails = nullptr;

		virtual bool ExecuteTask() override;
	};

#pragma endregion
}
