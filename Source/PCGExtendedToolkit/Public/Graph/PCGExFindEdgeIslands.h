// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExGraphProcessor.h"
#include "Data/PCGExData.h"

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
		bool bCrossing = false;
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

	struct FCrossing
	{
		FCrossing()
		{
		}

		int32 EdgeA = -1;
		int32 EdgeB = -1;
		FVector Center;
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
				if (!Edge.bValid) { continue; } // Crossing may invalidate edges.
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

	struct FCrossingsHandler
	{
		mutable FRWLock CrossingLock;

		FNetwork* Network;
		double Tolerance;
		double SquaredTolerance;

		TArray<FBox> SegmentBounds;
		TArray<FCrossing> Crossings;

		int32 NumEdges;
		int32 StartIndex = 0;

		FCrossingsHandler(FNetwork* InNetwork, double InTolerance)
			: Network(InNetwork),
			  Tolerance(InTolerance),
			  SquaredTolerance(InTolerance * InTolerance)
		{
			NumEdges = InNetwork->Edges.Num();

			Crossings.Empty();

			SegmentBounds.Empty();
			SegmentBounds.Reserve(NumEdges);
		}

		~FCrossingsHandler()
		{
			SegmentBounds.Empty();
			Crossings.Empty();
			Network = nullptr;
		}

		void Prepare(const TArray<FPCGPoint>& InPoints)
		{
			for (int i = 0; i < NumEdges; i++)
			{
				const FUnsignedEdge& Edge = Network->Edges[i];
				FBox& NewBox = SegmentBounds.Emplace_GetRef(EForceInit::ForceInit);
				NewBox += InPoints[Edge.Start].Transform.GetLocation();
				NewBox += InPoints[Edge.End].Transform.GetLocation();
			}
		}

		void ProcessEdge(int32 EdgeIndex, const TArray<FPCGPoint>& InPoints)
		{
			TArray<FUnsignedEdge>& Edges = Network->Edges;

			const FUnsignedEdge& Edge = Edges[EdgeIndex];
			const FBox CurrentBox = SegmentBounds[EdgeIndex].ExpandBy(Tolerance);
			const FVector A1 = InPoints[Edge.Start].Transform.GetLocation();
			const FVector B1 = InPoints[Edge.End].Transform.GetLocation();

			for (int i = 0; i < NumEdges; i++)
			{
				if (CurrentBox.Intersect(SegmentBounds[i]))
				{
					const FUnsignedEdge& OtherEdge = Edges[i];
					FVector A2 = InPoints[OtherEdge.Start].Transform.GetLocation();
					FVector B2 = InPoints[OtherEdge.End].Transform.GetLocation();
					FVector A3;
					FVector B3;
					FMath::SegmentDistToSegment(A1, B1, A2, B2, A3, B3);
					const bool bIsEnd = A1 == A3 || B1 == A3 || A2 == A3 || B2 == A3 || A1 == B3 || B1 == B3 || A2 == B3 || B2 == B3;
					if (!bIsEnd && FVector::DistSquared(A3, B3) < SquaredTolerance)
					{
						FWriteScopeLock WriteLock(CrossingLock);
						FCrossing& Crossing = Crossings.Emplace_GetRef();
						Crossing.EdgeA = EdgeIndex;
						Crossing.EdgeB = i;
						Crossing.Center = FMath::Lerp(A3, B3, 0.5);
					}
				}
			}
		}

		void InsertCrossings()
		{
			TArray<FNode>& Nodes = Network->Nodes;
			TArray<FUnsignedEdge>& Edges = Network->Edges;

			Nodes.Reserve(Nodes.Num() + Crossings.Num());
			StartIndex = Nodes.Num();
			int32 Index = StartIndex;
			for (const FCrossing& Crossing : Crossings)
			{
				Edges[Crossing.EdgeA].bValid = false;
				Edges[Crossing.EdgeB].bValid = false;

				FNode& NewNode = Nodes.Emplace_GetRef();
				NewNode.Index = Index++;
				NewNode.Edges.Reserve(4);
				NewNode.bCrossing = true;

				Network->InsertEdge(FUnsignedEdge(NewNode.Index, Edges[Crossing.EdgeA].Start, EPCGExEdgeType::Complete));
				Network->InsertEdge(FUnsignedEdge(NewNode.Index, Edges[Crossing.EdgeA].End, EPCGExEdgeType::Complete));
				Network->InsertEdge(FUnsignedEdge(NewNode.Index, Edges[Crossing.EdgeB].Start, EPCGExEdgeType::Complete));
				Network->InsertEdge(FUnsignedEdge(NewNode.Index, Edges[Crossing.EdgeB].End, EPCGExEdgeType::Complete));
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
	virtual FName GetMainOutputLabel() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	virtual int32 GetPreferredChunkSize() const override;

	virtual PCGExData::EInit GetMainOutputInitMode() const override;

public:
	/** Edge types to crawl to create a Island */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExEdgeType"))
	uint8 CrawlEdgeTypes = static_cast<uint8>(EPCGExEdgeType::Complete);

	/** Removes roaming points from the output, and keeps only points that are part of an island. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bPruneIsolatedPoints = true;

	/** Output one point data per island, rather than one single data set containing possibly disconnected islands. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	//bool bOutputIndividualIslands = true;

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

	/** If two edges are close enough, create a "crossing" point. !!! VERY EXPENSIVE !!! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bFindCrossings = false;

	/** Distance at which segments are considered crossing. !!! VERY EXPENSIVE !!! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bFindCrossings"))
	double CrossingTolerance = 10;

	/** Edges will inherit point attributes -- NOT IMPLEMENTED*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bInheritAttributes = false;

	/** Name of the attribute to ouput the unique Island identifier to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName IslandIDAttributeName = "IslandID";

	/** Name of the attribute to output the Island size to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName IslandSizeAttributeName = "IslandSize";

	/** Cleanup graph socket data from output points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bDeleteGraphData = true;

private:
	friend class FPCGExFindEdgeIslandsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFindEdgeIslandsContext : public FPCGExGraphProcessorContext
{
	friend class FPCGExFindEdgeIslandsElement;

	virtual ~FPCGExFindEdgeIslandsContext() override;

	EPCGExEdgeType CrawlEdgeTypes;
	//bool bOutputIndividualIslands;
	bool bPruneIsolatedPoints;
	bool bInheritAttributes;

	int32 MinIslandSize;
	int32 MaxIslandSize;

	bool bFindCrossings;
	double CrossingTolerance;

	int32 IslandUIndex = 0;

	TMap<int32, int32> IndexRemap;

	FName IslandIDAttributeName;
	FName IslandSizeAttributeName;
	FName PointUIDAttributeName;

	mutable FRWLock NetworkLock;
	PCGExGraph::FNetwork* Network = nullptr;
	PCGExGraph::FCrossingsHandler* Crossings = nullptr;
	PCGExData::FPointIOGroup* IslandsIO;

	PCGExData::FKPointIOMarkedBindings<int32>* Markings = nullptr;

	EPCGExRoamingResolveMethod ResolveRoamingMethod;

	FPCGMetadataAttribute<int64>* InCachedIndex;
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

class PCGEXTENDEDTOOLKIT_API FWriteIslandTask : public FPCGExNonAbandonableTask
{
public:
	FWriteIslandTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
	                 PCGExData::FPointIO* InIslandIO, PCGExGraph::FNetwork* InNetwork, TMap<int32, int32>* InIndexRemap = nullptr) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		IslandIO(InIslandIO), Network(InNetwork), IndexRemap(InIndexRemap)
	{
	}

	PCGExData::FPointIO* IslandIO = nullptr;
	PCGExGraph::FNetwork* Network = nullptr;
	TMap<int32, int32>* IndexRemap = nullptr;

	virtual bool ExecuteTask() override;
};
