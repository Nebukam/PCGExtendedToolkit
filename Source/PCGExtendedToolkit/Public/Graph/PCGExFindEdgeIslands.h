// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExGraphProcessor.h"

#include "PCGExFindEdgeIslands.generated.h"

UENUM(BlueprintType)
enum class EPCGExRoamingResolveMethod : uint8
{
	Overlap UMETA(DisplayName = "Overlap", ToolTip="Roaming nodes with unidirectional connections will create their own overlapping Islands."),
	Merge UMETA(DisplayName = "Merge", ToolTip="Roaming Islands will be merged into existing ones; thus creating less Islands yet not canon ones."),
	Cutoff UMETA(DisplayName = "Cutoff", ToolTip="Roaming Islands discovery will be cut off where they would otherwise overlap."),
};

namespace PCGExGraph
{
	struct FNode
	{
		FNode()
		{
		}

		int32 Index = -1;
		int32 Island = -1;
		TArray<int32> Edges;

		bool IsIsolated() const { return Island == -1; }

		bool GetNeighbors(TArray<int32>& OutIndices, const TArray<FUnsignedEdge>& InEdges)
		{
			if (Edges.IsEmpty()) { return false; }
			for (const int32 Edge : Edges) { OutIndices.Add(InEdges[Edge].Other(Index)); }
			return true;
		}

		template <typename Func, typename... Args>
		void CallbackOnNeighbors(const TArray<FUnsignedEdge>& InEdges, Func Callback, Args&&... InArgs)
		{
			for (const int32 Edge : Edges) { Callback(InEdges[Edge].Other(Index, std::forward<Args>(InArgs)...)); }
		}

		void AddEdge(const int32 Edge)
		{
			Edges.AddUnique(Edge);
		}
	};

	struct FNetwork
	{
		mutable FRWLock NetworkLock;

		const int32 NumEdgesReserve;
		int32 IslandIncrement = 0;
		int32 NumIslands = 0;
		int32 NumEdges = 0;

		TArray<FNode> Nodes;
		TSet<uint64> UniqueEdges;
		TArray<FUnsignedEdge> Edges;
		TMap<int32, int32> IslandSizes;


		~FNetwork()
		{
			Nodes.Empty();
			UniqueEdges.Empty();
			Edges.Empty();
			IslandSizes.Empty();
		}

		FNetwork(const int32 InNumEdgesReserve, const int32 InNumNodes)
			: NumEdgesReserve(InNumEdgesReserve)
		{
			Nodes.SetNum(InNumNodes);

			int32 Index = 0;

			for (FNode& Node : Nodes)
			{
				Node.Index = Index++;
				Node.Edges.Reserve(NumEdgesReserve);
			}
		}

		bool InsertEdge(FUnsignedEdge Edge)
		{
			const uint64 Hash = Edge.GetUnsignedHash();

			{
				FReadScopeLock ReadLock(NetworkLock);
				if (UniqueEdges.Contains(Hash)) { return false; }
			}


			FWriteScopeLock WriteLock(NetworkLock);
			UniqueEdges.Add(Hash);
			const int32 EdgeIndex = Edges.Add(Edge);


			FNode& NodeA = Nodes[Edge.Start];
			FNode& NodeB = Nodes[Edge.End];

			NodeA.AddEdge(EdgeIndex);
			NodeB.AddEdge(EdgeIndex);

			if (NodeA.Island == -1 && NodeB.Island == -1)
			{
				// New island
				NumIslands++;
				NodeA.Island = NodeB.Island = IslandIncrement++;
			}
			else if (NodeA.Island != -1 && NodeB.Island != -1)
			{
				if (NodeA.Island != NodeB.Island)
				{
					// Merge islands
					NumIslands--;
					MergeIsland(NodeB.Index, NodeA.Island);
				}
			}
			else
			{
				// Expand island
				NodeA.Island = NodeB.Island = FMath::Max(NodeA.Island, NodeB.Island);
			}

			return true;
		}

		void MergeIsland(int32 NodeIndex, int32 Island)
		{
			FNode& Node = Nodes[NodeIndex];
			if (Node.Island == Island) { return; }
			Node.Island = Island;
			for (const int32 EdgeIndex : Node.Edges)
			{
				MergeIsland(Edges[EdgeIndex].Other(NodeIndex), Island);
			}
		}

		void PrepareIslands(int32 MinSize = 1, int32 MaxSize = TNumericLimits<int32>::Max())
		{
			UniqueEdges.Empty();
			IslandSizes.Empty(NumIslands);
			NumEdges = 0;

			for (const FUnsignedEdge& Edge : Edges)
			{
				FNode& NodeA = Nodes[Edge.Start];
				if (const int32* SizePtr = IslandSizes.Find(NodeA.Island); !SizePtr) { IslandSizes.Add(NodeA.Island, 1); }
				else { IslandSizes.Add(NodeA.Island, *SizePtr + 1); }
			}

			for (TPair<int32, int32>& Pair : IslandSizes)
			{
				if (FMath::IsWithin(Pair.Value, MinSize, MaxSize)) { NumEdges += Pair.Value; }
				else { Pair.Value = -1; }
			}
		}
	};
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExFindEdgeIslandsSettings : public UPCGExGraphProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindEdgeIslands, "Graph : Find Edge Islands", "Create partitions from interconnected points. Each Island is the result of all input graphs combined.");
#endif

	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	virtual int32 GetPreferredChunkSize() const override;

	virtual PCGExData::EInit GetMainOutputInitMode() const override;

public:
	/** Edge types to crawl to create a Island */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExEdgeType"))
	uint8 CrawlEdgeTypes = static_cast<uint8>(EPCGExEdgeType::Complete);

	/** Output one point data per island, rather than one single data set containing possibly disconnected islands. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bOutputIndividualIslands = true;

	/** Don't output Islands if they have less points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveSmallIslands = true;

	/** Minimum points threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bRemoveSmallIslands"))
	int32 MinIslandSize = 3;

	/** Don't output Islands if they have more points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveBigIslands = false;

	/** Maximum points threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bRemoveBigIslands"))
	int32 MaxIslandSize = 500;

	/** Name of the attribute to ouput the unique Island identifier to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName IslandIDAttributeName = "IslandID";

	/** Name of the attribute to output the Island size to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName IslandSizeAttributeName = "IslandSize";

	/** Not implemented yet, always Overlap */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExRoamingResolveMethod ResolveRoamingMethod = EPCGExRoamingResolveMethod::Overlap;

private:
	friend class FPCGExFindEdgeIslandsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFindEdgeIslandsContext : public FPCGExGraphProcessorContext
{
	friend class FPCGExFindEdgeIslandsElement;

	virtual ~FPCGExFindEdgeIslandsContext() override;

	EPCGExEdgeType CrawlEdgeTypes;
	bool bOutputIndividualIslands;
	int32 MinIslandSize;
	int32 MaxIslandSize;

	int32 IslandUIndex = 0;

	FName IslandIDAttributeName;
	FName IslandSizeAttributeName;
	FName PointUIDAttributeName;

	mutable FRWLock NetworkLock;
	PCGExGraph::FNetwork* Network = nullptr;
	PCGExData::FPointIOGroup* IslandsIO;

	EPCGExRoamingResolveMethod ResolveRoamingMethod;

	FPCGMetadataAttribute<int64>* InCachedIndex;

	void CenterEdgePoint(FPCGPoint& Point, PCGExGraph::FUnsignedEdge Edge) const;
};


class PCGEXTENDEDTOOLKIT_API FPCGExFindEdgeIslandsElement : public FPCGExGraphProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

class PCGEXTENDEDTOOLKIT_API FInsertEdgeTask : public FPCGExNonAbandonableTask
{
public:
	FInsertEdgeTask(FPCGExAsyncManager* InManager, const PCGExMT::FTaskInfos& InInfos, PCGExData::FPointIO* InPointIO) :
		FPCGExNonAbandonableTask(InManager, InInfos, InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};

class PCGEXTENDEDTOOLKIT_API FWriteIslandTask : public FPCGExNonAbandonableTask
{
public:
	FWriteIslandTask(FPCGExAsyncManager* InManager, const PCGExMT::FTaskInfos& InInfos, PCGExData::FPointIO* InPointIO,
	                 PCGExData::FPointIO* InIslandData) :
		FPCGExNonAbandonableTask(InManager, InInfos, InPointIO),
		IslandData(InIslandData)
	{
	}

	PCGExData::FPointIO* IslandData;

	virtual bool ExecuteTask() override;
};
