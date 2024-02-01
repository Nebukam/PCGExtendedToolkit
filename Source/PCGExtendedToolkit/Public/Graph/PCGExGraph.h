// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Data/PCGExAttributeHelpers.h"
#include "PCGExMT.h"
#include "PCGExEdge.h"
#include "Data/PCGExData.h"

#include "PCGExGraph.generated.h"

struct FPCGExPointsProcessorContext;

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExGraphBuilderSettings
{
	GENERATED_BODY()

	/** Removes roaming points from the output, and keeps only points that are part of an cluster. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (PCG_Overridable))
	bool bPruneIsolatedPoints = true;

	/** Don't output Clusters if they have less points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteEdgePosition = true;

	/** Edge position interpolation between start and end point positions. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (PCG_Overridable, EditCondition="bWriteEdgePosition"))
	double EdgePosition = 0.5;

	/** Don't output Clusters if they have less points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveSmallClusters = false;

	/** Minimum points threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (PCG_Overridable, EditCondition="bRemoveSmallClusters", ClampMin=2))
	int32 MinClusterSize = 3;

	/** Don't output Clusters if they have more points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveBigClusters = false;

	/** Maximum points threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (PCG_Overridable, EditCondition="bRemoveBigClusters", ClampMin=2))
	int32 MaxClusterSize = 500;

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
	const FName SourceParamsLabel = TEXT("Graph");
	const FName OutputParamsLabel = TEXT("➜");

	const FName SourceGraphsLabel = TEXT("In");
	const FName OutputGraphsLabel = TEXT("Out");

	const FName SourceVerticesLabel = TEXT("Vtx");
	const FName OutputVerticesLabel = TEXT("Vtx");

	const FName SourcePathsLabel = TEXT("Paths");
	const FName OutputPathsLabel = TEXT("Paths");

	constexpr PCGExMT::AsyncState State_ReadyForNextGraph = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessingGraph = __COUNTER__;

	constexpr PCGExMT::AsyncState State_CachingGraphIndices = __COUNTER__;
	constexpr PCGExMT::AsyncState State_SwappingGraphIndices = __COUNTER__;

	constexpr PCGExMT::AsyncState State_FindingEdgeTypes = __COUNTER__;

	constexpr PCGExMT::AsyncState State_BuildCustomGraph = __COUNTER__;
	constexpr PCGExMT::AsyncState State_FindingCrossings = __COUNTER__;
	constexpr PCGExMT::AsyncState State_WritingClusters = __COUNTER__;
	constexpr PCGExMT::AsyncState State_WaitingOnWritingClusters = __COUNTER__;

	constexpr PCGExMT::AsyncState State_PromotingEdges = __COUNTER__;

#pragma region Graph

	class FGraph;

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

		FSubGraph()
		{
		}

		~FSubGraph()
		{
			Nodes.Empty();
			Edges.Empty();
		}

		void Add(const FIndexedEdge& Edge, FGraph* InGraph);
		void Invalidate(FGraph* InGraph);
	};

	class PCGEXTENDEDTOOLKIT_API FGraph
	{
		mutable FRWLock GraphLock;
		const int32 NumEdgesReserve;

	public:
		bool bRequiresConsolidation = false;

		TArray<FNode> Nodes;
		TArray<FIndexedEdge> Edges;

		TSet<uint64> UniqueEdges;

		TArray<FSubGraph*> SubGraphs;

		bool bWriteEdgePosition = true;
		double EdgePosition = 0.5;

		FGraph(const int32 InNumNodes, const int32 InNumEdgesReserve = 10)
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
		void InsertEdges(const TArray<FUnsignedEdge>& InEdges);
		void InsertEdges(const TArray<FIndexedEdge>& InEdges);

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
			Nodes.Empty();
			UniqueEdges.Empty();
			Edges.Empty();

			for (const FSubGraph* Cluster : SubGraphs) { delete Cluster; }
			SubGraphs.Empty();
		}
	};

	class PCGEXTENDEDTOOLKIT_API FGraphBuilder
	{
	public:
		FPCGExGraphBuilderSettings* OutputSettings = nullptr;

		bool bPrunePoints = false;
		FString EdgeTagValue;

		PCGExData::FPointIO* PointIO = nullptr;

		FGraph* Graph = nullptr;

		PCGExData::FPointIOGroup* EdgesIO = nullptr;
		PCGExData::FPointIO* SourceEdgesIO = nullptr;

		bool bCompiledSuccessfully = false;

		FGraphBuilder(PCGExData::FPointIO& InPointIO, FPCGExGraphBuilderSettings* InSettings, const int32 NumEdgeReserve = 6, PCGExData::FPointIO* InSourceEdges = nullptr)
			: OutputSettings(InSettings), SourceEdgesIO(InSourceEdges)
		{
			PointIO = &InPointIO;
			PointIO->Tags->Set(Tag_Cluster, PointIO->GetInOut()->UID, EdgeTagValue);

			const int32 NumNodes = PointIO->GetOutNum();

			Graph = new FGraph(NumNodes, NumEdgeReserve);
			Graph->bWriteEdgePosition = OutputSettings->bWriteEdgePosition;
			Graph->EdgePosition = OutputSettings->EdgePosition;

			EdgesIO = new PCGExData::FPointIOGroup();
			EdgesIO->DefaultOutputLabel = OutputEdgesLabel;

			bPrunePoints = OutputSettings->bPruneIsolatedPoints;
		}

		void Compile(FPCGExPointsProcessorContext* InContext) const;
		void Write(FPCGExPointsProcessorContext* InContext) const;

		~FGraphBuilder()
		{
			PCGEX_DELETE(Graph)
			PCGEX_DELETE(EdgesIO)
		}
	};

	static bool GetRemappedIndices(PCGExData::FPointIO& InPointIO, const FName AttributeName, TMap<int32, int32>& OutIndices)
	{
		OutIndices.Empty();

		PCGEx::TFAttributeReader<int32>* IndexReader = new PCGEx::TFAttributeReader<int32>(AttributeName);
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

	static bool GetRemappedIndices(const PCGExData::FPointIO& InPointIO, const FName AttributeName, TMap<int32, int32>& OutIndices)
	{
		return GetRemappedIndices(const_cast<PCGExData::FPointIO&>(InPointIO), AttributeName, OutIndices);
	}

	static bool IsPointDataVtxReady(const UPCGPointData* PointData)
	{
		const FName Tags[] = {Tag_EdgeIndex, Tag_EdgesNum};
		constexpr int16 I32 = static_cast<uint16>(EPCGMetadataTypes::Integer32);

		for (const FName Name : Tags)
		{
			if (const FPCGMetadataAttributeBase* AttributeCheck = PointData->Metadata->GetMutableAttribute(Name);
				!AttributeCheck || AttributeCheck->GetTypeId() != I32) { return false; }
		}

		return true;
	}

	static bool IsPointDataEdgeReady(const UPCGPointData* PointData)
	{
		const FName Tags[] = {Tag_EdgeStart, Tag_EdgeEnd};
		constexpr int16 I32 = static_cast<uint16>(EPCGMetadataTypes::Integer32);

		for (const FName Name : Tags)
		{
			if (const FPCGMetadataAttributeBase* AttributeCheck = PointData->Metadata->GetMutableAttribute(Name);
				!AttributeCheck || AttributeCheck->GetTypeId() != I32) { return false; }
		}

		return true;
	}

#pragma endregion
}

class PCGEXTENDEDTOOLKIT_API FPCGExWriteSubGraphEdgesTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExWriteSubGraphEdgesTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
	                             PCGExData::FPointIO* InClusterIO, PCGExGraph::FGraph* InGraph, PCGExGraph::FSubGraph* InSubGraph)
		: FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		  EdgeIO(InClusterIO), Graph(InGraph), SubGraph(InSubGraph)
	{
	}

	PCGExData::FPointIO* EdgeIO = nullptr;
	PCGExGraph::FGraph* Graph = nullptr;
	PCGExGraph::FSubGraph* SubGraph = nullptr;

	virtual bool ExecuteTask() override;
};


class PCGEXTENDEDTOOLKIT_API FPCGExCompileGraphTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExCompileGraphTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
	                       PCGExGraph::FGraphBuilder* InGraphBuilder, const int32 InMin = 1, const int32 InMax = TNumericLimits<int32>::Max())
		: FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		  Builder(InGraphBuilder), Min(InMin), Max(InMax)
	{
	}

	PCGExGraph::FGraphBuilder* Builder = nullptr;
	int32 Min;
	int32 Max;

	virtual bool ExecuteTask() override;
};
