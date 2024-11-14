// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExEdge.h"
#include "PCGExGraph.h"
#include "PCGExSorting.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Geometry/PCGExGeo.h"

#include "PCGExCluster.generated.h"

namespace PCGExCluster
{
	struct FExpandedNode;
	struct FExpandedEdge;
}

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Cluster Closest Search Mode")--E*/)
enum class EPCGExClusterClosestSearchMode : uint8
{
	Node = 0 UMETA(DisplayName = "Closest node", ToolTip="Proximity to node position"),
	Edge = 1 UMETA(DisplayName = "Closest edge", ToolTip="Proximity to edge, then endpoint"),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExNodeSelectionDetails
{
	GENERATED_BODY()

	FPCGExNodeSelectionDetails()
	{
	}

	explicit FPCGExNodeSelectionDetails(const double InMaxDistance):
		MaxDistance(InMaxDistance)
	{
	}

	~FPCGExNodeSelectionDetails()
	{
	}

	/** Drives how the seed & goal points are selected within each cluster. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExClusterClosestSearchMode PickingMethod = EPCGExClusterClosestSearchMode::Edge;

	/** Max distance at which a node can be selected. Use <= 0 to ignore distance check. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=-1))
	double MaxDistance = -1;

	// TODO : Support local attribute

	FORCEINLINE bool WithinDistance(const FVector& NodePosition, const FVector& TargetPosition) const
	{
		if (MaxDistance <= 0) { return true; }
		return FVector::Distance(NodePosition, TargetPosition) < MaxDistance;
	}
};

namespace PCGExCluster
{
	const FName OutputNodeFlagLabel = TEXT("Flag");
	const FName SourceNodeFlagLabel = TEXT("Node Flags");

	struct /*PCGEXTENDEDTOOLKIT_API*/ FAdjacencyData
	{
		int32 NodeIndex = -1;
		int32 NodePointIndex = -1;
		int32 EdgeIndex = -1;
		FVector Direction = FVector::OneVector;
		double Length = 0;
	};

	class FCluster;

	struct /*PCGEXTENDEDTOOLKIT_API*/ FNode : public PCGExGraph::FNode
	{
		FNode() = default;

		FNode(const int32 InNodeIndex, const int32 InPointIndex):
			PCGExGraph::FNode(InNodeIndex, InPointIndex)
		{
		}

		FVector GetCentroid(const FCluster* InCluster) const;
		void ComputeNormal(const FCluster* InCluster, const TArray<FAdjacencyData>& AdjacencyData, FVector& OutNormal) const;
		int32 ValidEdges(const FCluster* InCluster);
		bool HasAnyValidEdges(const FCluster* InCluster);
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FExpandedNeighbor
	{
		const FNode* Node;
		const PCGExGraph::FIndexedEdge* Edge;
		FVector Direction;

		FExpandedNeighbor(const FNode* InNode, const PCGExGraph::FIndexedEdge* InEdge, const FVector& InDirection):
			Node(InNode), Edge(InEdge), Direction(InDirection)
		{
		}

		FExpandedNeighbor():
			Node(nullptr), Edge(nullptr), Direction(FVector::ZeroVector)
		{
		}

		FExpandedNeighbor(const FExpandedNeighbor& Other):
			Node(Other.Node), Edge(Other.Edge), Direction(Other.Direction)
		{
		}
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FCluster : public TSharedFromThis<FCluster>
	{
	protected:
		bool bIsMirror = false;

		bool bEdgeLengthsDirty = true;
		bool bIsCopyCluster = false;

		mutable FRWLock ClusterLock;

	public:
		int32 NumRawVtx = 0;
		int32 NumRawEdges = 0;

		bool bValid = false;
		bool bIsOneToOne = false; // Whether the input data has a single set of edges for a single set of vtx

		int32 ClusterID = -1;
		TSharedPtr<PCGEx::FIndexLookup> NodeIndexLookup; // Point Index -> Node index
		//TMap<uint64, int32> EdgeIndexLookup;   // Edge Hash -> Edge Index
		TSharedPtr<TArray<FNode>> Nodes;
		TSharedPtr<TArray<FExpandedNode>> ExpandedNodes;
		TSharedPtr<TArray<FExpandedEdge>> ExpandedEdges;
		TSharedPtr<TArray<PCGExGraph::FIndexedEdge>> Edges;
		TSharedPtr<TArray<double>> EdgeLengths;
		TArray<FVector> NodePositions;

		FBox Bounds;

		const TArray<FPCGPoint>* VtxPoints = nullptr;

		TWeakPtr<PCGExData::FPointIO> VtxIO;
		TWeakPtr<PCGExData::FPointIO> EdgesIO;

		TSharedPtr<PCGEx::FIndexedItemOctree> NodeOctree;
		TSharedPtr<PCGEx::FIndexedItemOctree> EdgeOctree;

		FCluster(const TSharedPtr<PCGExData::FPointIO>& InVtxIO, const TSharedPtr<PCGExData::FPointIO>& InEdgesIO,
		         const TSharedPtr<PCGEx::FIndexLookup>& InNodeIndexLookup);
		FCluster(const TSharedRef<FCluster>& OtherCluster,
		         const TSharedPtr<PCGExData::FPointIO>& InVtxIO, const TSharedPtr<PCGExData::FPointIO>& InEdgesIO,
		         const TSharedPtr<PCGEx::FIndexLookup>& InNodeIndexLookup,
		         bool bCopyNodes, bool bCopyEdges, bool bCopyLookup);

		void ClearInheritedForChanges(const bool bClearOwned = false);
		void WillModifyVtxIO(const bool bClearOwned = false);
		void WillModifyVtxPositions(const bool bClearOwned = false);

		~FCluster();

		bool BuildFrom(
			const TMap<uint32, int32>& InEndpointsLookup,
			const TArray<int32>* InExpectedAdjacency,
			const PCGExData::ESource PointsSource = PCGExData::ESource::In);

		void BuildFrom(const PCGExGraph::FSubGraph* SubGraph);

		bool IsValidWith(const TSharedRef<PCGExData::FPointIO>& InVtxIO, const TSharedRef<PCGExData::FPointIO>& InEdgesIO) const;

		FORCEINLINE FNode* GetNode(const int32 Index) const { return (Nodes->GetData() + Index); }
		FORCEINLINE double GetNodePointIndex(const int32 Index) const { return (Nodes->GetData() + Index)->PointIndex; }
		FORCEINLINE const FPCGPoint* GetNodePoint(const int32 Index) const { return (VtxPoints->GetData() + (Nodes->GetData() + Index)->PointIndex); }
		FORCEINLINE PCGExGraph::FIndexedEdge* GetEdge(const int32 Index) const { return (Edges->GetData() + Index); }

		FORCEINLINE FNode* GetEdgeStart(const PCGExGraph::FIndexedEdge& InEdge) const { return (Nodes->GetData() + NodeIndexLookup->Get(InEdge.Start)); }
		FORCEINLINE FNode* GetEdgeStart(const int32 InEdgeIndex) const { return (Nodes->GetData() + NodeIndexLookup->Get((Edges->GetData() + InEdgeIndex)->Start)); }
		FORCEINLINE FNode* GetEdgeEnd(const PCGExGraph::FIndexedEdge& InEdge) const { return (Nodes->GetData() + NodeIndexLookup->Get(InEdge.End)); }
		FORCEINLINE FNode* GetEdgeEnd(const int32 InEdgeIndex) const { return (Nodes->GetData() + NodeIndexLookup->Get((Edges->GetData() + InEdgeIndex)->End)); }
		FORCEINLINE FNode* GetEdgeOtherNode(const int32 InEdgeIndex, const int32 InNodeIndex) const { return (Nodes->GetData() + NodeIndexLookup->Get((Edges->GetData() + InEdgeIndex)->Other((Nodes->GetData() + InNodeIndex)->PointIndex))); }

		FORCEINLINE FVector GetStartPos(const PCGExGraph::FIndexedEdge& InEdge) const { return *(NodePositions.GetData() + NodeIndexLookup->Get(InEdge.Start)); }
		FORCEINLINE FVector GetStartPos(const int32 InEdgeIndex) const { return GetStartPos(*(Edges->GetData() + InEdgeIndex)); }
		FORCEINLINE FVector GetEndPos(const PCGExGraph::FIndexedEdge& InEdge) const { return *(NodePositions.GetData() + NodeIndexLookup->Get(InEdge.End)); }
		FORCEINLINE FVector GetEndPos(const int32 InEdgeIndex) const { return GetEndPos(*(Edges->GetData() + InEdgeIndex)); }

		FORCEINLINE FVector GetPos(const FNode& InNode) const { return *(NodePositions.GetData() + InNode.NodeIndex); }
		FORCEINLINE FVector GetPos(const FNode* InNode) const { return *(NodePositions.GetData() + InNode->NodeIndex); }
		FORCEINLINE FVector GetPos(const int32 Index) const { return *(NodePositions.GetData() + Index); }
		FORCEINLINE FVector GetPos(const FExpandedNeighbor& InNode) const { return *(NodePositions.GetData() + InNode.Node->NodeIndex); }
		FORCEINLINE FVector GetPos(const FExpandedNeighbor* InNode) const { return *(NodePositions.GetData() + InNode->Node->NodeIndex); }

		FORCEINLINE double GetDist(const PCGExGraph::FIndexedEdge& InEdge) const { return FVector::Dist((*(NodePositions.GetData() + NodeIndexLookup->Get(InEdge.Start))), (*(NodePositions.GetData() + NodeIndexLookup->Get(InEdge.End)))); }
		FORCEINLINE double GetDist(const int32 InEdgeIndex) const { return GetDist(*(Edges->GetData() + InEdgeIndex)); }
		FORCEINLINE double GetDist(const int32 NodeA, const int32 NodeB) const { return FVector::Dist(*(NodePositions.GetData() + NodeA), *(NodePositions.GetData() + NodeB)); }
		FORCEINLINE double GetDist(const FNode& A, const FNode& B) const { return GetDist(A.NodeIndex, B.NodeIndex); }
		FORCEINLINE double GetDistSquared(const PCGExGraph::FIndexedEdge& InEdge) const { return FVector::DistSquared((*(NodePositions.GetData() + NodeIndexLookup->Get(InEdge.Start))), (*(NodePositions.GetData() + NodeIndexLookup->Get(InEdge.End)))); }
		FORCEINLINE double GetDistSquared(const int32 InEdgeIndex) const { return GetDist(*(Edges->GetData() + InEdgeIndex)); }
		FORCEINLINE double GetDistSquared(const int32 NodeA, const int32 NodeB) const { return FVector::DistSquared(*(NodePositions.GetData() + NodeA), *(NodePositions.GetData() + NodeB)); }
		FORCEINLINE double GetDistSquared(const FNode& A, const FNode& B) const { return GetDistSquared(A.NodeIndex, B.NodeIndex); }

		FORCEINLINE FNode* GetRoamingNode(const FVector& UVW) const { return GetNode(FindClosestNode(Bounds.GetCenter() + Bounds.GetExtent() * UVW, EPCGExClusterClosestSearchMode::Edge)); }

		FORCEINLINE double EdgeDistToEdge(const uint64 A, const uint64 B, FVector& OutP1, FVector& OutP2) const
		{
			FMath::SegmentDistToSegment(
				GetPos(PCGEx::H64A(A)), GetPos(PCGEx::H64B(A)),
				GetPos(PCGEx::H64A(B)), GetPos(PCGEx::H64B(B)),
				OutP1, OutP2);

			return FVector::Dist(OutP1, OutP2);
		}

		FORCEINLINE double EdgeDistToEdgeSquared(const uint64 A, const uint64 B, FVector& OutP1, FVector& OutP2) const
		{
			FMath::SegmentDistToSegment(
				GetPos(PCGEx::H64A(A)), GetPos(PCGEx::H64B(A)),
				GetPos(PCGEx::H64A(B)), GetPos(PCGEx::H64B(B)),
				OutP1, OutP2);

			return FVector::DistSquared(OutP1, OutP2);
		}

		FORCEINLINE FVector GetDir(const int32 FromNode, const int32 ToNode) const
		{
			return ((*(NodePositions.GetData() + ToNode)) - (*(NodePositions.GetData() + FromNode))).GetSafeNormal();
		}

		FORCEINLINE FVector GetDir(const FNode& From, const FNode& To) const { return GetDir(From.NodeIndex, To.NodeIndex); }

		FORCEINLINE FVector GetEdgeDir(const PCGExGraph::FIndexedEdge& InEdge) const
		{
			return ((*(NodePositions.GetData() + NodeIndexLookup->Get(InEdge.Start))) - (*(NodePositions.GetData() + NodeIndexLookup->Get(InEdge.End)))).GetSafeNormal();
		}

		void RebuildNodeOctree();
		void RebuildEdgeOctree();
		void RebuildOctree(EPCGExClusterClosestSearchMode Mode, const bool bForceRebuild = false);

		int32 FindClosestNode(const FVector& Position, EPCGExClusterClosestSearchMode Mode, const int32 MinNeighbors = 0) const;
		int32 FindClosestNode(const FVector& Position, const int32 MinNeighbors = 0) const;
		int32 FindClosestNodeFromEdge(const FVector& Position, const int32 MinNeighbors = 0) const;

		int32 FindClosestEdge(const int32 InNodeIndex, const FVector& InPosition) const;

		int32 FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, int32 MinNeighborCount = 1) const;
		int32 FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, const TSet<int32>& Exclusion, int32 MinNeighborCount = 1) const;

		void ComputeEdgeLengths(bool bNormalize = false);

		void GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth) const;
		void GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth, const TSet<int32>& Skip) const;

		void GetConnectedEdges(const int32 FromNodeIndex, TArray<int32>& OutNodeIndices, TArray<int32>& OutEdgeIndices, const int32 SearchDepth) const;
		void GetConnectedEdges(const int32 FromNodeIndex, TArray<int32>& OutNodeIndices, TArray<int32>& OutEdgeIndices, const int32 SearchDepth, const TSet<int32>& SkipNodes, const TSet<int32>& SkipEdges) const;

		FORCEINLINE FVector GetEdgeDirection(const int32 FromIndex, const int32 ToIndex) const { return (GetPos(FromIndex) - GetPos(ToIndex)).GetSafeNormal(); }
		FORCEINLINE FVector GetClosestPointOnEdge(const int32 FromIndex, const int32 ToIndex, const FVector& Position) const
		{
			return FMath::ClosestPointOnSegment(Position, GetPos(FromIndex), GetPos(ToIndex));
		}

		FORCEINLINE FVector GetClosestPointOnEdge(const PCGExGraph::FIndexedEdge& InEdge, const FVector& Position) const
		{
			return FMath::ClosestPointOnSegment(
				Position,
				*(NodePositions.GetData() + NodeIndexLookup->Get(InEdge.Start)),
				*(NodePositions.GetData() + NodeIndexLookup->Get(InEdge.End)));
		}

		FORCEINLINE FVector GetClosestPointOnEdge(const int32 EdgeIndex, const FVector& Position) const
		{
			return GetClosestPointOnEdge(*(Edges->GetData() + EdgeIndex), Position);
		}

		FORCEINLINE double GetPointDistToEdgeSquared(const PCGExGraph::FIndexedEdge& InEdge, const FVector& Position) const
		{
			return FMath::PointDistToSegmentSquared(
				Position,
				*(NodePositions.GetData() + NodeIndexLookup->Get(InEdge.Start)),
				*(NodePositions.GetData() + NodeIndexLookup->Get(InEdge.End)));
		}

		FORCEINLINE double GetPointDistToEdgeSquared(const int32 EdgeIndex, const FVector& Position) const
		{
			return GetPointDistToEdgeSquared(*(Edges->GetData() + EdgeIndex), Position);
		}

		FORCEINLINE FVector GetCentroid(const int32 NodeIndex) const
		{
			const FNode* Node = (Nodes->GetData() + NodeIndex);
			FVector Centroid = FVector::ZeroVector;
			for (const uint64 AdjacencyHash : Node->Adjacency) { Centroid += GetPos(PCGEx::H64A(AdjacencyHash)); }
			return Centroid / static_cast<double>(Node->Adjacency.Num());
		}

		void GetValidEdges(TArray<PCGExGraph::FIndexedEdge>& OutValidEdges) const;

		int32 FindClosestNeighborInDirection(const int32 NodeIndex, const FVector& Direction, int32 MinNeighborCount = 1) const;

		TSharedPtr<TArray<FExpandedNode>> GetExpandedNodes(const bool bBuild);
		void ExpandNodes(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager);

		TSharedPtr<TArray<FExpandedEdge>> GetExpandedEdges(const bool bBuild);
		void ExpandEdges(PCGExMT::FTaskManager* AsyncManager);

		template <typename T, class MakeFunc>
		void GrabNeighbors(const int32 NodeIndex, TArray<T>& OutNeighbors, const MakeFunc&& Make) const
		{
			FNode* Node = (Nodes->GetData() + NodeIndex);
			PCGEx::InitArray(OutNeighbors, Node->Adjacency.Num());
			for (int i = 0; i < Node->Adjacency.Num(); i++)
			{
				uint32 OtherNodeIndex;
				uint32 EdgeIndex;
				PCGEx::H64(Node->Adjacency[i], OtherNodeIndex, EdgeIndex);
				OutNeighbors[i] = Make(Node, (Nodes->GetData() + OtherNodeIndex), (Edges->GetData() + EdgeIndex));
			}
		}

		template <typename T, class MakeFunc>
		void GrabNeighbors(const FNode& Node, TArray<T>& OutNeighbors, const MakeFunc&& Make) const
		{
			PCGEx::InitArray(OutNeighbors, Node.Adjacency.Num());
			for (int i = 0; i < Node.Adjacency.Num(); i++)
			{
				uint32 OtherNodeIndex;
				uint32 EdgeIndex;
				PCGEx::H64(Node.Adjacency[i], OtherNodeIndex, EdgeIndex);
				OutNeighbors[i] = Make((Nodes->GetData() + OtherNodeIndex), (Edges->GetData() + EdgeIndex));
			}
		}

		void UpdatePositions();

	protected:
		FORCEINLINE FNode& GetOrCreateNodeUnsafe(const TArray<FPCGPoint>& InNodePoints, int32 PointIndex)
		{
			int32 NodeIndex = NodeIndexLookup->Get(PointIndex);

			if (NodeIndex != -1) { return *(Nodes->GetData() + NodeIndex); }

			NodeIndex = Nodes->Num();
			NodeIndexLookup->GetMutable(PointIndex) = NodeIndex;

			FNode& NewNode = Nodes->Emplace_GetRef(NodeIndex, PointIndex);
			const FVector Pos = InNodePoints[PointIndex].Transform.GetLocation();
			NodePositions.Add(Pos);
			Bounds += Pos;

			return NewNode;
		}
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FExpandedNode
	{
		const FNode* Node = nullptr;
		TArray<FExpandedNeighbor> Neighbors;

		FExpandedNode(const TSharedPtr<FCluster>& Cluster, const int32 InNodeIndex):
			Node(Cluster->Nodes->GetData() + InNodeIndex)
		{
			const int32 NumNeighbors = Node->Adjacency.Num();
			const FVector Pos = Cluster->GetPos(InNodeIndex);
			Neighbors.SetNum(NumNeighbors);
			for (int i = 0; i < Neighbors.Num(); i++)
			{
				uint32 NodeIndex;
				uint32 EdgeIndex;
				PCGEx::H64(Node->Adjacency[i], NodeIndex, EdgeIndex);
				Neighbors[i] = FExpandedNeighbor(
					Cluster->Nodes->GetData() + NodeIndex, Cluster->Edges->GetData() + EdgeIndex,
					(Cluster->GetPos(NodeIndex) - Pos).GetSafeNormal());
			}
		}

		FExpandedNode(const FExpandedNode& Other):
			Node(Other.Node), Neighbors(Other.Neighbors)
		{
		}

		FExpandedNode():
			Node(nullptr)
		{
		}

		~FExpandedNode() = default;

		operator int32() const { return Node->NodeIndex; }
		
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FExpandedEdge
	{
		int32 Index;
		const FNode* Start;
		const FNode* End;
		FBoxSphereBounds BSB;

		FExpandedEdge(const FCluster* Cluster, const int32 InEdgeIndex):
			Index(InEdgeIndex),
			Start(Cluster->GetEdgeStart(InEdgeIndex)),
			End(Cluster->GetEdgeEnd(InEdgeIndex)),
			BSB(FBoxSphereBounds(FSphere(FMath::Lerp(Cluster->GetPos(Start), Cluster->GetPos(End), 0.5), FVector::Dist(Cluster->GetPos(Start), Cluster->GetPos(End)) * 0.5)))
		{
		}

		FExpandedEdge():
			Index(-1), Start(nullptr), End(nullptr), BSB(FBoxSphereBounds(ForceInit))
		{
		}

		FExpandedEdge(const FExpandedEdge& Other):
			Index(Other.Index), Start(Other.Start), End(Other.End), BSB(Other.BSB)
		{
		}

		~FExpandedEdge() = default;

		FORCEINLINE uint64 GetNodes() const { return PCGEx::H64(Start->NodeIndex, End->NodeIndex); }
		FORCEINLINE double GetEdgeLength(const FCluster* Cluster) const { return FVector::Dist(Cluster->GetPos(Start), Cluster->GetPos(End)); }
		FORCEINLINE double GetEdgeLengthSquared(const FCluster* Cluster) const { return FVector::DistSquared(Cluster->GetPos(Start), Cluster->GetPos(End)); }
		FORCEINLINE FVector GetCenter() const { return BSB.Origin; }
		FORCEINLINE int32 OtherNodeIndex(const int32 NodeIndex) const
		{
			check(NodeIndex == Start->NodeIndex || NodeIndex == End->NodeIndex)
			return NodeIndex == Start->NodeIndex ? End->NodeIndex : Start->NodeIndex;
		}

		bool operator==(const FExpandedEdge& ExpandedEdge) const
		{
			return (Start == ExpandedEdge.Start && End == ExpandedEdge.End) || (Start == ExpandedEdge.End && End == ExpandedEdge.Start);
		};
	};

	static void GetAdjacencyData(const FCluster* InCluster, FNode& InNode, TArray<FAdjacencyData>& OutData)
	{
		const int32 NumAdjacency = InNode.Adjacency.Num();
		const FVector NodePosition = InCluster->GetPos(InNode);
		OutData.Reserve(NumAdjacency);
		for (int i = 0; i < NumAdjacency; i++)
		{
			uint32 NIndex;
			uint32 EIndex;

			PCGEx::H64(InNode.Adjacency[i], NIndex, EIndex);

			const FNode* OtherNode = InCluster->Nodes->GetData() + NIndex;
			const FVector OtherPosition = InCluster->GetPos(OtherNode);

			FAdjacencyData& Data = OutData.Emplace_GetRef();
			Data.NodeIndex = NIndex;
			Data.NodePointIndex = OtherNode->PointIndex;
			Data.EdgeIndex = EIndex;
			Data.Direction = (NodePosition - OtherPosition).GetSafeNormal();
			Data.Length = FVector::Dist(NodePosition, OtherPosition);
		}
	}
}

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExEdgeDirectionSettings
{
	GENERATED_BODY()

	/** Method to pick the edge direction amongst various possibilities.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExEdgeDirectionMethod DirectionMethod = EPCGExEdgeDirectionMethod::EndpointsOrder;

	/** Attribute picker for the selected Direction Method.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="DirectionMethod==EPCGExEdgeDirectionMethod::EdgeDotAttribute", EditConditionHides))
	FPCGAttributePropertyInputSelector DirSourceAttribute;

	/** Further refine the direction method. Not all methods make use of this property.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExEdgeDirectionChoice DirectionChoice = EPCGExEdgeDirectionChoice::SmallestToGreatest;

	bool bAscendingDesired = false;
	TSharedPtr<PCGExData::TBuffer<FVector>> EdgeDirReader;

	TSharedPtr<PCGExSorting::PointSorter<true>> Sorter;

	void RegisterBuffersDependencies(
		FPCGExContext* InContext,
		PCGExData::FFacadePreloader& FacadePreloader,
		const TArray<FPCGExSortRuleConfig>* InSortingRules = nullptr) const;

	bool Init(
		FPCGExContext* InContext,
		const TSharedRef<PCGExData::FFacade>& InVtxDataFacade,
		const TArray<FPCGExSortRuleConfig>* InSortingRules = nullptr);

	bool InitFromParent(
		FPCGExContext* InContext,
		const FPCGExEdgeDirectionSettings& ParentSettings,
		const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade);

	bool RequiresSortingRules() const { return DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsSort; }
	bool RequiresEndpointsMetadata() const { return DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsSort; }
	bool RequiresEdgeMetadata() const { return DirectionMethod == EPCGExEdgeDirectionMethod::EdgeDotAttribute; }

	bool SortEndpoints(const PCGExCluster::FCluster* InCluster, PCGExGraph::FIndexedEdge& InEdge) const;
};
