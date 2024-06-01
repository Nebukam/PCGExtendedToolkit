// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Data/PCGExAttributeHelpers.h"
#include "PCGExMT.h"
#include "PCGExEdge.h"
#include "PCGExPointsProcessor.h"
#include "PCGExSettings.h"
#include "Data/PCGExData.h"
#include "Data/Blending/PCGExMetadataBlender.h"

#include "PCGExGraph.generated.h"

struct FPCGExPointsProcessorContext;

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Graph Value Source"))
enum class EPCGExGraphValueSource : uint8
{
	Point UMETA(DisplayName = "Point", Tooltip="Value is fetched from the point being evaluated."),
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
struct PCGEXTENDEDTOOLKIT_API FPCGExGraphBuilderSettings
{
	GENERATED_BODY()

	/** Removes roaming points from the output, and keeps only points that are part of an cluster. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable))
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
	int32 MinClusterSize = 3;

	/** Don't output Clusters if they have more points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveBigClusters = false;

	/** Maximum points threshold */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable, EditCondition="bRemoveBigClusters", ClampMin=2))
	int32 MaxClusterSize = 500;

	/** Refresh Edge Seed. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable))
	bool bRefreshEdgeSeed = false;

	int32 GetMinClusterSize() const { return bRemoveSmallClusters ? MinClusterSize : 0; }
	int32 GetMaxClusterSize() const { return bRemoveBigClusters ? MaxClusterSize : TNumericLimits<int32>::Max(); }
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExEdgeCrawlingSettingsOverride
{
	GENERATED_BODY()

	/** Name of the custom graph params */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName Identifier = "GraphIdentifier";

	/** Edge types to crawl for these params */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExEdgeType"))
	uint8 EdgeTypes = static_cast<uint8>(EPCGExEdgeType::Complete);
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExEdgeCrawlingSettings
{
	GENERATED_BODY()

	/** Edge types to crawl to create a Cluster */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExEdgeType"))
	uint8 DefaultEdgeTypes = static_cast<uint8>(EPCGExEdgeType::Complete);

	/** Overrides */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, TitleProperty="{Identifier}"))
	TArray<FPCGExEdgeCrawlingSettingsOverride> Overrides;

	uint8 GetCrawlingEdgeTypes(const FName Identifier)
	{
		if (Overrides.IsEmpty()) { return DefaultEdgeTypes; }
		for (const FPCGExEdgeCrawlingSettingsOverride& Override : Overrides) { if (Override.Identifier == Identifier) { return Override.EdgeTypes; } }
		return DefaultEdgeTypes;
	}
};

namespace PCGExGraph
{
	const FName SourceSocketOverrideParamsLabel = TEXT("Ctrl Socket");
	const FName SourceSocketParamsLabel = TEXT("Sockets");
	const FName OutputSocketParamsLabel = TEXT("Socket");

	const FName OutputSocketStateLabel = TEXT("SocketState");
	const FName SourceSocketStateLabel = TEXT("SocketStates");

	const FName SourceSingleGraphLabel = TEXT("Graph");
	const FName OutputForwardGraphsLabel = TEXT("➜");

	const FName SourceGraphsLabel = TEXT("In");
	const FName OutputGraphsLabel = TEXT("Out");

	const FName SourceVerticesLabel = TEXT("Vtx");
	const FName OutputVerticesLabel = TEXT("Vtx");

	const FName SourcePathsLabel = TEXT("Paths");
	const FName OutputPathsLabel = TEXT("Paths");

	const FName Tag_PackedClusterPointCount = TEXT("PCGEx/PackedClusterPointCount");
	const FName Tag_PackedClusterEdgeCount = TEXT("PCGEx/PackedClusterEdgeCount");

	constexpr PCGExMT::AsyncState State_ReadyForNextGraph = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessingGraph = __COUNTER__;

	constexpr PCGExMT::AsyncState State_CachingGraphIndices = __COUNTER__;
	constexpr PCGExMT::AsyncState State_SwappingGraphIndices = __COUNTER__;

	constexpr PCGExMT::AsyncState State_FindingEdgeTypes = __COUNTER__;

	constexpr PCGExMT::AsyncState State_BuildCustomGraph = __COUNTER__;
	constexpr PCGExMT::AsyncState State_FindingCrossings = __COUNTER__;
	constexpr PCGExMT::AsyncState State_WritingClusters = __COUNTER__;
	constexpr PCGExMT::AsyncState State_WaitingOnWritingClusters = __COUNTER__;

	constexpr PCGExMT::AsyncState State_FindingPointEdgeIntersections = __COUNTER__;
	constexpr PCGExMT::AsyncState State_InsertingPointEdgeIntersections = __COUNTER__;

	constexpr PCGExMT::AsyncState State_FindingEdgeEdgeIntersections = __COUNTER__;
	constexpr PCGExMT::AsyncState State_InsertingEdgeEdgeIntersections = __COUNTER__;

	constexpr PCGExMT::AsyncState State_PromotingEdges = __COUNTER__;
	constexpr PCGExMT::AsyncState State_UpdatingCompoundCenters = __COUNTER__;

	constexpr PCGExMT::AsyncState State_MergingPointCompounds = __COUNTER__;
	constexpr PCGExMT::AsyncState State_MergingEdgeCompounds = __COUNTER__;
	constexpr PCGExMT::AsyncState State_BlendingPointEdgeCrossings = __COUNTER__;
	constexpr PCGExMT::AsyncState State_BlendingEdgeEdgeCrossings = __COUNTER__;

	constexpr PCGExMT::AsyncState State_WritingMainState = __COUNTER__;
	constexpr PCGExMT::AsyncState State_WritingStatesAttributes = __COUNTER__;
	constexpr PCGExMT::AsyncState State_WritingIndividualStates = __COUNTER__;

	class FGraph;

#pragma region Graph

	struct PCGEXTENDEDTOOLKIT_API FGraphMetadataSettings
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

		void Grab(const FPCGContext* Context, const FPCGExPointPointIntersectionSettings& Settings)
		{
			bWriteCompounded = Settings.bWriteCompounded;
			CompoundedAttributeName = Settings.CompoundedAttributeName;
			PCGEX_SOFT_VALIDATE_NAME(bWriteCompounded, CompoundedAttributeName, Context)

			bWriteCompoundSize = Settings.bWriteCompoundSize;
			CompoundSizeAttributeName = Settings.CompoundSizeAttributeName;
			PCGEX_SOFT_VALIDATE_NAME(bWriteCompoundSize, CompoundSizeAttributeName, Context)
		}

		void Grab(const FPCGContext* Context, const FPCGExEdgeEdgeIntersectionSettings& Settings)
		{
			bWriteCrossing = Settings.bWriteCrossing;
			CrossingAttributeName = Settings.CrossingAttributeName;
			bFlagCrossing = Settings.bFlagCrossing;
			PCGEX_SOFT_VALIDATE_NAME(bFlagCrossing, FlagA, Context)
			PCGEX_SOFT_VALIDATE_NAME(bFlagCrossing, FlagB, Context)
		}

		void Grab(const FPCGContext* Context, const FPCGExPointEdgeIntersectionSettings& Settings)
		{
			bWriteIntersector = Settings.bWriteIntersector;
			IntersectorAttributeName = Settings.IntersectorAttributeName;
			PCGEX_SOFT_VALIDATE_NAME(bWriteIntersector, IntersectorAttributeName, Context)
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FGraphNodeMetadata
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

	struct PCGEXTENDEDTOOLKIT_API FGraphEdgeMetadata
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

	struct PCGEXTENDEDTOOLKIT_API FNode
	{
		FNode()
		{
		}

		bool bValid = true;

		int32 NodeIndex = -1;  // Index in the context of the list that helds the node
		int32 PointIndex = -1; // Index in the context of the UPCGPointData that helds the vtx
		int32 NumExportedEdges = 0;

		TArray<int32> Edges;

		~FNode()
		{
			Edges.Empty();
		}

		void Add(const int32 EdgeIndex);
	};

	struct PCGEXTENDEDTOOLKIT_API FSubGraph
	{
		int64 Id = -1;
		TSet<int32> Nodes;
		TSet<int32> Edges; //TODO : Test for TArray
		TSet<int32> EdgesInIOIndices;
		PCGExData::FPointIO* PointIO = nullptr;

		FSubGraph()
		{
		}

		~FSubGraph()
		{
			Nodes.Empty();
			Edges.Empty();
			EdgesInIOIndices.Empty();
			PointIO = nullptr;
		}

		void Add(const FIndexedEdge& Edge, FGraph* InGraph);
		void Invalidate(FGraph* InGraph);
		int32 GetFirstInIOIndex();
	};

	class PCGEXTENDEDTOOLKIT_API FGraph
	{
		mutable FRWLock GraphLock;
		const int32 NumEdgesReserve;

	public:
		bool bRequiresConsolidation = false;

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
			Nodes.SetNum(InNumNodes);
			for (int i = 0; i < InNumNodes; i++)
			{
				FNode& Node = Nodes[i];
				Node.NodeIndex = Node.PointIndex = i;
				Node.Edges.Reserve(NumEdgesReserve);
			}
		}

		bool InsertEdge(const int32 A, const int32 B, FIndexedEdge& OutEdge);
		bool InsertEdge(const FIndexedEdge& Edge);
		void InsertEdges(const TSet<uint64>& InEdges, int32 InIOIndex);
		void InsertEdges(const TArray<uint64>& InEdges, int32 InIOIndex);
		void InsertEdges(const TArray<FUnsignedEdge>& InEdges, int32 InIOIndex);
		void InsertEdges(const TArray<FIndexedEdge>& InEdges);

		TArrayView<FNode> AddNodes(const int32 NumNewNodes);

		void BuildSubGraphs(const int32 Min = 1, const int32 Max = TNumericLimits<int32>::Max());

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

	class PCGEXTENDEDTOOLKIT_API FGraphBuilder
	{
	public:
		FPCGExGraphBuilderSettings* OutputSettings = nullptr;

		bool bPrunePoints = false;
		int64 PairId;
		FString PairIdStr;

		PCGExData::FPointIO* PointIO = nullptr;

		FGraph* Graph = nullptr;

		PCGExData::FPointIOCollection* EdgesIO = nullptr;
		PCGExData::FPointIOCollection* SourceEdgesIO = nullptr;

		bool bCompiledSuccessfully = false;

		FGraphBuilder(PCGExData::FPointIO& InPointIO, FPCGExGraphBuilderSettings* InSettings, const int32 NumEdgeReserve = 6, PCGExData::FPointIOCollection* InSourceEdges = nullptr)
			: OutputSettings(InSettings), SourceEdgesIO(InSourceEdges)
		{
			PointIO = &InPointIO;
			PairId = PointIO->GetOutIn()->UID;
			PointIO->Tags->Set(TagStr_ClusterPair, PairId, PairIdStr);

			const int32 NumNodes = PointIO->GetOutNum();

			Graph = new FGraph(NumNodes, NumEdgeReserve);
			Graph->bWriteEdgePosition = OutputSettings->bWriteEdgePosition;
			Graph->EdgePosition = OutputSettings->EdgePosition;
			Graph->bRefreshEdgeSeed = OutputSettings->bRefreshEdgeSeed;

			EdgesIO = new PCGExData::FPointIOCollection();
			EdgesIO->DefaultOutputLabel = OutputEdgesLabel;

			bPrunePoints = OutputSettings->bPruneIsolatedPoints;
		}

		void Compile(FPCGExPointsProcessorContext* InContext, FGraphMetadataSettings* MetadataSettings = nullptr) const;
		void Write(FPCGExPointsProcessorContext* InContext) const;

		~FGraphBuilder()
		{
			PCGEX_DELETE(Graph)
			PCGEX_DELETE(EdgesIO)
		}
	};

	static bool GetRemappedIndices(PCGExData::FPointIO& InPointIO, const FName AttributeName, TMap<int64, int32>& OutIndices)
	{
		OutIndices.Empty();

		PCGEx::TFAttributeReader<int64>* IndexReader = new PCGEx::TFAttributeReader<int64>(AttributeName);
		if (!IndexReader->Bind(InPointIO))
		{
			PCGEX_DELETE(IndexReader)
			return false;
		}

		OutIndices.Reserve(IndexReader->Values.Num());
		for (int i = 0; i < IndexReader->Values.Num(); i++) { OutIndices.Add(IndexReader->Values[i], i); }

		PCGEX_DELETE(IndexReader)
		return true;
	}

	static bool GetRemappedIndices(const PCGExData::FPointIO& InPointIO, const FName AttributeName, TMap<int64, int32>& OutIndices)
	{
		return GetRemappedIndices(const_cast<PCGExData::FPointIO&>(InPointIO), AttributeName, OutIndices);
	}

#pragma endregion

	static bool IsPointDataVtxReady(const UPCGPointData* PointData)
	{
		const FName Tags[] = {Tag_EdgeIndex, Tag_EdgesNum};
		constexpr int16 I64 = static_cast<uint16>(EPCGMetadataTypes::Integer64);
		constexpr int16 I32 = static_cast<uint16>(EPCGMetadataTypes::Integer32);

		for (const FName Name : Tags)
		{
			if (const FPCGMetadataAttributeBase* AttributeCheck = PointData->Metadata->GetMutableAttribute(Name);
				!AttributeCheck || (AttributeCheck->GetTypeId() != I64 && AttributeCheck->GetTypeId() != I32)) { return false; }
		}

		return true;
	}

	static bool IsPointDataEdgeReady(const UPCGPointData* PointData)
	{
		const FName Tags[] = {Tag_EdgeStart, Tag_EdgeEnd};
		constexpr int16 I64 = static_cast<uint16>(EPCGMetadataTypes::Integer64);
		constexpr int16 I32 = static_cast<uint16>(EPCGMetadataTypes::Integer32);

		for (const FName Name : Tags)
		{
			if (const FPCGMetadataAttributeBase* AttributeCheck = PointData->Metadata->GetMutableAttribute(Name);
				!AttributeCheck || (AttributeCheck->GetTypeId() != I64 && AttributeCheck->GetTypeId() != I32)) { return false; }
		}

		return true;
	}

	static bool GetReducedVtxIndices(PCGExData::FPointIO& InEdges, const TMap<int64, int32>* NodeIndicesMap, TArray<int32>& OutVtxIndices, int32& OutEdgeNum)
	{
		PCGEx::TFAttributeReader<int64>* StartIndexReader = new PCGEx::TFAttributeReader<int64>(Tag_EdgeStart);
		PCGEx::TFAttributeReader<int64>* EndIndexReader = new PCGEx::TFAttributeReader<int64>(Tag_EdgeEnd);
		const bool bStart = StartIndexReader->Bind(InEdges);
		const bool bEnd = EndIndexReader->Bind(InEdges);

		if (!bStart || !bEnd ||
			StartIndexReader->Values.Num() != EndIndexReader->Values.Num())
		{
			PCGEX_DELETE(StartIndexReader)
			PCGEX_DELETE(EndIndexReader)
			return false;
		}

		OutEdgeNum = StartIndexReader->Values.Num();

		OutVtxIndices.Empty();
		OutVtxIndices.Reserve(OutEdgeNum * 2);

		for (int i = 0; i < OutEdgeNum; i++)
		{
			const int32* NodeStartPtr = NodeIndicesMap->Find(StartIndexReader->Values[i]);
			const int32* NodeEndPtr = NodeIndicesMap->Find(EndIndexReader->Values[i]);

			if (!NodeStartPtr || !NodeEndPtr || (*NodeStartPtr == *NodeEndPtr)) { continue; }

			OutVtxIndices.AddUnique(*NodeStartPtr);
			OutVtxIndices.AddUnique(*NodeEndPtr);
		}

		PCGEX_DELETE(StartIndexReader)
		PCGEX_DELETE(EndIndexReader)
		return true;
	}

	static void CleanupVtxData(const PCGExData::FPointIO* PointIO)
	{
		UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;
		PointIO->Tags->Remove(TagStr_ClusterPair);
		Metadata->DeleteAttribute(Tag_EdgesNum);
		Metadata->DeleteAttribute(Tag_EdgeIndex);
		Metadata->DeleteAttribute(Tag_EdgeStart);
		Metadata->DeleteAttribute(Tag_EdgeEnd);
	}
}

namespace PCGExGraphTask
{
#pragma region Graph tasks

	static void WriteSubGraphEdges(
		const TArray<FPCGPoint>& Vertices,
		PCGExGraph::FGraph* Graph,
		PCGExGraph::FSubGraph* SubGraph,
		const PCGExGraph::FGraphMetadataSettings* MetadataSettings)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FWriteSubGraphEdges::ExecuteTask);

		PCGExData::FPointIO& EdgeIO = *SubGraph->PointIO;

		TArray<FPCGPoint>& MutablePoints = EdgeIO.GetOut()->GetMutablePoints();
		MutablePoints.SetNum(SubGraph->Edges.Num());

		int32 PointIndex = 0;
		if (EdgeIO.GetIn())
		{
			// Copy any existing point properties first
			const TArray<FPCGPoint>& InPoints = EdgeIO.GetIn()->GetPoints();
			for (const int32 EdgeIndex : SubGraph->Edges)
			{
				const int32 EdgePtIndex = Graph->Edges[EdgeIndex].PointIndex;
				if (InPoints.IsValidIndex(EdgePtIndex)) { MutablePoints[PointIndex] = InPoints[EdgePtIndex]; }
				PointIndex++;
			}
		}

		EdgeIO.SetNumInitialized(SubGraph->Edges.Num());
		EdgeIO.CreateOutKeys();

		PCGEx::TFAttributeWriter<int64>* EdgeStart = new PCGEx::TFAttributeWriter<int64>(PCGExGraph::Tag_EdgeStart, -1, false);
		PCGEx::TFAttributeWriter<int64>* EdgeEnd = new PCGEx::TFAttributeWriter<int64>(PCGExGraph::Tag_EdgeEnd, -1, false);

		EdgeStart->BindAndGet(EdgeIO);
		EdgeEnd->BindAndGet(EdgeIO);

		PointIndex = 0;
		for (const int32 EdgeIndex : SubGraph->Edges)
		{
			const PCGExGraph::FIndexedEdge& Edge = Graph->Edges[EdgeIndex];
			FPCGPoint& Point = MutablePoints[PointIndex];

			EdgeStart->Values[PointIndex] = Vertices[Graph->Nodes[Edge.Start].PointIndex].MetadataEntry;
			EdgeEnd->Values[PointIndex] = Vertices[Graph->Nodes[Edge.End].PointIndex].MetadataEntry;

			if (Point.Seed == 0) { PCGExMath::RandomizeSeed(Point); }

			if (PCGExGraph::FGraphEdgeMetadata** EdgeMetaPtr = Graph->EdgeMetadata.Find(EdgeIndex))
			{
				PCGExGraph::FGraphEdgeMetadata* EdgeMeta = *EdgeMetaPtr;
				//if()
				//int32 ParentCompoundIndex = PCGExGraph::FGraphEdgeMetadata::GetParentIndex();
				//TODO: Handle edge metadata	
			}

			PointIndex++;
		}

		if (Graph->bWriteEdgePosition)
		{
			PointIndex = 0;
			for (const int32 EdgeIndex : SubGraph->Edges)
			{
				const PCGExGraph::FIndexedEdge& Edge = Graph->Edges[EdgeIndex];
				MutablePoints[PointIndex].Transform.SetLocation(
					FMath::Lerp(
						Vertices[Graph->Nodes[Edge.Start].PointIndex].Transform.GetLocation(),
						Vertices[Graph->Nodes[Edge.End].PointIndex].Transform.GetLocation(),
						Graph->EdgePosition));
				PointIndex++;
			}
		}

		if (Graph->bRefreshEdgeSeed)
		{
			const FVector SeedOffset = FVector(EdgeIO.IOIndex);
			for (FPCGPoint& Point : MutablePoints) { PCGExMath::RandomizeSeed(Point, SeedOffset); }
		}

		EdgeStart->Write();
		EdgeEnd->Write();

		PCGEX_DELETE(EdgeStart)
		PCGEX_DELETE(EdgeEnd)

		if (MetadataSettings &&
			!Graph->EdgeMetadata.IsEmpty() &&
			!Graph->NodeMetadata.IsEmpty())
		{
			if (MetadataSettings->bFlagCrossing)
			{
				// Need to go through each point and add flags matching edges
				for (const int32 NodeIndex : SubGraph->Nodes)
				{
					PCGExGraph::FNode Node = Graph->Nodes[NodeIndex];
					PCGExGraph::FGraphNodeMetadata** NodeMetaPtr = Graph->NodeMetadata.Find(NodeIndex);

					if (!NodeMetaPtr || (*NodeMetaPtr)->Type != EPCGExIntersectionType::EdgeEdge)
					{
					}
				}
			}
		}

		EdgeIO.Flatten();
	};

	class PCGEXTENDEDTOOLKIT_API FWriteSubGraphEdges : public FPCGExNonAbandonableTask
	{
	public:
		FWriteSubGraphEdges(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		                    PCGExGraph::FGraph* InGraph,
		                    PCGExGraph::FSubGraph* InSubGraph,
		                    PCGExGraph::FGraphMetadataSettings* InMetadataSettings = nullptr)
			: FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			  Graph(InGraph),
			  SubGraph(InSubGraph),
			  MetadataSettings(InMetadataSettings)
		{
		}

		PCGExGraph::FGraph* Graph = nullptr;
		PCGExGraph::FSubGraph* SubGraph = nullptr;

		PCGExGraph::FGraphMetadataSettings* MetadataSettings = nullptr;

		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FCompileGraph : public FPCGExNonAbandonableTask
	{
	public:
		FCompileGraph(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		              PCGExGraph::FGraphBuilder* InGraphBuilder,
		              const int32 InMin = 1,
		              const int32 InMax = TNumericLimits<int32>::Max(),
		              PCGExGraph::FGraphMetadataSettings* InMetadataSettings = nullptr)
			: FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			  Builder(InGraphBuilder),
			  Min(InMin),
			  Max(InMax),
			  MetadataSettings(InMetadataSettings)
		{
		}

		PCGExGraph::FGraphBuilder* Builder = nullptr;
		int32 Min;
		int32 Max;

		PCGExGraph::FGraphMetadataSettings* MetadataSettings = nullptr;

		virtual bool ExecuteTask() override;
	};

#pragma endregion
}
