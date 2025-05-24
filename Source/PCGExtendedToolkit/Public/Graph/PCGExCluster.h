// Copyright 2025 Timothé Lapetite and contributors
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
	struct FBoundedEdge;
}

UENUM()
enum class EPCGExClusterClosestSearchMode : uint8
{
	Vtx  = 0 UMETA(DisplayName = "Closest vtx", ToolTip="Proximity to node position"),
	Edge = 1 UMETA(DisplayName = "Closest edge", ToolTip="Proximity to edge, then endpoint"),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExNodeSelectionDetails
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

	bool WithinDistance(const FVector& NodePosition, const FVector& TargetPosition) const;
};

namespace PCGExCluster
{
	using PCGExGraph::FLink;
	using PCGExGraph::FEdge;

	const FName OutputNodeFlagLabel = TEXT("Flag");
	const FName SourceNodeFlagLabel = TEXT("NodeFlags");

	struct PCGEXTENDEDTOOLKIT_API FAdjacencyData
	{
		int32 NodeIndex = -1;
		int32 NodePointIndex = -1;
		int32 EdgeIndex = -1;
		FVector Direction = FVector::OneVector;
		double Length = 0;
	};

	class FCluster;

	struct PCGEXTENDEDTOOLKIT_API FNode : PCGExGraph::FNode
	{
		FNode() = default;

		FNode(const int32 InNodeIndex, const int32 InPointIndex);

		FVector GetCentroid(const FCluster* InCluster) const;
		void ComputeNormal(const FCluster* InCluster, const TArray<FAdjacencyData>& AdjacencyData, FVector& OutNormal) const;
		int32 ValidEdges(const FCluster* InCluster);
		bool HasAnyValidEdges(const FCluster* InCluster);
	};

	struct PCGEXTENDEDTOOLKIT_API FBoundedEdge
	{
		int32 Index;
		FBoxSphereBounds Bounds;

		FBoundedEdge(const FCluster* Cluster, const int32 InEdgeIndex);
		FBoundedEdge();

		~FBoundedEdge() = default;

		bool operator==(const FBoundedEdge& ExpandedEdge) const { return (Index == ExpandedEdge.Index && Bounds == ExpandedEdge.Bounds); };
	};

	class PCGEXTENDEDTOOLKIT_API FCluster : public TSharedFromThis<FCluster>
	{
	protected:
		bool bIsMirror = false;

		bool bEdgeLengthsDirty = true;
		TSharedPtr<FCluster> OriginalCluster = nullptr;

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
		TSharedPtr<TArray<FBoundedEdge>> BoundedEdges;
		TSharedPtr<TArray<FEdge>> Edges;
		TSharedPtr<TArray<double>> EdgeLengths;
		TConstPCGValueRange<FTransform> VtxTransforms;

		FBox Bounds;

		const UPCGBasePointData* VtxPoints = nullptr;

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

		class TConstVtxLookup
		{
		public:
			TConstVtxLookup(const TSharedPtr<FCluster>& InCluster) : NodesArray(*InCluster->Nodes.Get())
			{
			}

			int32 Num() const { return NodesArray.Num(); }
			const int32& operator[](const int32 Index) const { return NodesArray[Index].PointIndex; }

		private:
			const TArray<FNode>& NodesArray;
		};

		class TVtxLookup
		{
		public:
			TVtxLookup(const TSharedPtr<FCluster>& InCluster) : NodesArray(*InCluster->Nodes.Get())
			{
			}

			int32 Num() const { return NodesArray.Num(); }

			int32& operator[](const int32 Index) { return NodesArray[Index].PointIndex; }
			const int32& operator[](const int32 Index) const { return NodesArray[Index].PointIndex; }

		private:
			TArray<FNode>& NodesArray;
		};

		void ClearInheritedForChanges(const bool bClearOwned = false);
		void WillModifyVtxIO(const bool bClearOwned = false);
		void WillModifyVtxPositions(const bool bClearOwned = false);

		~FCluster();

		bool BuildFrom(
			const TMap<uint32, int32>& InEndpointsLookup,
			const TArray<int32>* InExpectedAdjacency);

		void BuildFrom(const TSharedRef<PCGExGraph::FSubGraph>& SubGraph);

		bool IsValidWith(const TSharedRef<PCGExData::FPointIO>& InVtxIO, const TSharedRef<PCGExData::FPointIO>& InEdgesIO) const;
		bool HasTag(const FString& InTag);

		FORCEINLINE FNode* GetNode(const int32 Index) const { return (Nodes->GetData() + Index); }
		FORCEINLINE FNode* GetNode(const FLink Lk) const { return (Nodes->GetData() + Lk.Node); }
		FORCEINLINE int32 GetNodePointIndex(const int32 Index) const { return (Nodes->GetData() + Index)->PointIndex; }
		FORCEINLINE int32 GetNodePointIndex(const FLink Lk) const { return (Nodes->GetData() + Lk.Node)->PointIndex; }
		FORCEINLINE FEdge* GetEdge(const int32 Index) const { return (Edges->GetData() + Index); }
		FORCEINLINE FEdge* GetEdge(const FLink Lk) const { return (Edges->GetData() + Lk.Edge); }

		FORCEINLINE FNode* GetEdgeStart(const FEdge* InEdge) const { return (Nodes->GetData() + NodeIndexLookup->Get(InEdge->Start)); }
		FORCEINLINE FNode* GetEdgeStart(const FEdge& InEdge) const { return (Nodes->GetData() + NodeIndexLookup->Get(InEdge.Start)); }
		FORCEINLINE FNode* GetEdgeStart(const int32 InEdgeIndex) const { return (Nodes->GetData() + NodeIndexLookup->Get((Edges->GetData() + InEdgeIndex)->Start)); }
		FORCEINLINE FNode* GetEdgeEnd(const FEdge* InEdge) const { return (Nodes->GetData() + NodeIndexLookup->Get(InEdge->End)); }
		FORCEINLINE FNode* GetEdgeEnd(const FEdge& InEdge) const { return (Nodes->GetData() + NodeIndexLookup->Get(InEdge.End)); }
		FORCEINLINE FNode* GetEdgeEnd(const int32 InEdgeIndex) const { return (Nodes->GetData() + NodeIndexLookup->Get((Edges->GetData() + InEdgeIndex)->End)); }
		FORCEINLINE FNode* GetEdgeOtherNode(const int32 InEdgeIndex, const int32 InNodeIndex) const { return (Nodes->GetData() + NodeIndexLookup->Get((Edges->GetData() + InEdgeIndex)->Other((Nodes->GetData() + InNodeIndex)->PointIndex))); }
		FORCEINLINE FNode* GetEdgeOtherNode(const FLink Lk) const { return (Nodes->GetData() + NodeIndexLookup->Get((Edges->GetData() + Lk.Edge)->Other((Nodes->GetData() + Lk.Node)->PointIndex))); }

		FORCEINLINE FVector GetStartPos(const FEdge& InEdge) const { return VtxTransforms[InEdge.Start].GetLocation(); }
		FORCEINLINE FVector GetStartPos(const FEdge* InEdge) const { return VtxTransforms[InEdge->Start].GetLocation(); }
		FORCEINLINE FVector GetStartPos(const int32 InEdgeIndex) const { return VtxTransforms[(Edges->GetData() + InEdgeIndex)->Start].GetLocation(); }
		FORCEINLINE FVector GetEndPos(const FEdge& InEdge) const { return VtxTransforms[InEdge.End].GetLocation(); }
		FORCEINLINE FVector GetEndPos(const FEdge* InEdge) const { return VtxTransforms[InEdge->End].GetLocation(); }
		FORCEINLINE FVector GetEndPos(const int32 InEdgeIndex) const { return VtxTransforms[(Edges->GetData() + InEdgeIndex)->End].GetLocation(); }

		FORCEINLINE FVector GetPos(const FNode& InNode) const { return VtxTransforms[InNode.PointIndex].GetLocation(); }
		FORCEINLINE FVector GetPos(const FNode* InNode) const { return VtxTransforms[InNode->PointIndex].GetLocation(); }
		FORCEINLINE FVector GetPos(const int32 Index) const { return VtxTransforms[(Nodes->GetData() + Index)->PointIndex].GetLocation(); }
		FORCEINLINE FVector GetPos(const FLink Lk) const { return VtxTransforms[(Nodes->GetData() + Lk.Node)->PointIndex].GetLocation(); }

		double GetDist(const FEdge* InEdge) const { return FVector::Dist(VtxTransforms[InEdge->Start].GetLocation(), VtxTransforms[InEdge->End].GetLocation()); }
		double GetDist(const FEdge& InEdge) const { return FVector::Dist(VtxTransforms[InEdge.Start].GetLocation(), VtxTransforms[InEdge.End].GetLocation()); }
		double GetDist(const int32 InEdgeIndex) const { return GetDist(*(Edges->GetData() + InEdgeIndex)); }
		double GetDist(const int32 NodeA, const int32 NodeB) const { return FVector::Dist(GetPos(NodeA), GetPos(NodeB)); }
		double GetDist(const FNode& A, const FNode& B) const { return FVector::Dist(VtxTransforms[A.PointIndex].GetLocation(), VtxTransforms[B.PointIndex].GetLocation()); }
		double GetDistSquared(const FEdge& InEdge) const { return FVector::DistSquared(VtxTransforms[InEdge.Start].GetLocation(), VtxTransforms[InEdge.End].GetLocation()); }
		double GetDistSquared(const int32 InEdgeIndex) const { return GetDist(*(Edges->GetData() + InEdgeIndex)); }
		double GetDistSquared(const int32 NodeA, const int32 NodeB) const { return FVector::DistSquared(GetPos(NodeA), GetPos(NodeB)); }
		double GetDistSquared(const FNode& A, const FNode& B) const { return FVector::DistSquared(VtxTransforms[A.PointIndex].GetLocation(), VtxTransforms[B.PointIndex].GetLocation()); }

		FNode* GetGuidedHalfEdge(const int32 Edge, const FVector& Guide, const FVector& Up = FVector::UpVector) const;

		FNode* GetRoamingNode(const FVector& UVW) const { return GetNode(FindClosestNode(Bounds.GetCenter() + Bounds.GetExtent() * UVW, EPCGExClusterClosestSearchMode::Edge)); }

		double EdgeDistToEdge(const FEdge* A, const FEdge* B, FVector& OutP1, FVector& OutP2) const;
		double EdgeDistToEdge(const int32 EdgeA, const int32 EdgeB, FVector& OutP1, FVector& OutP2) const;
		double EdgeDistToEdgeSquared(const FEdge* A, const FEdge* B, FVector& OutP1, FVector& OutP2) const;
		double EdgeDistToEdgeSquared(const int32 EdgeA, const int32 EdgeB, FVector& OutP1, FVector& OutP2) const;
		FVector GetDir(const int32 FromNode, const int32 ToNode) const;
		FVector GetDir(const FNode& From, const FNode& To) const;
		double GetEdgeLength(const FEdge& InEdge) const;
		double GetEdgeLengthSquared(const FEdge& InEdge) const;
		FVector GetEdgeDir(const FEdge& InEdge) const;
		FVector GetEdgeDir(const int32 InEdgeIndex) const;
		FVector GetEdgeDir(const FLink Lk) const;
		FVector GetEdgeDir(const int32 InEdgeIndex, const int32 InStartPtIndex) const;
		FVector GetEdgeDir(const FLink Lk, const int32 InStartPtIndex) const;

		TSharedPtr<PCGEx::FIndexedItemOctree> GetNodeOctree();
		TSharedPtr<PCGEx::FIndexedItemOctree> GetEdgeOctree();

		void RebuildNodeOctree();
		void RebuildEdgeOctree();
		void RebuildOctree(EPCGExClusterClosestSearchMode Mode, const bool bForceRebuild = false);

		void GatherNodesPointIndices(TArray<int32>& OutValidNodesPointIndices, const bool bValidity) const;

		template <int32 MinNeighbors = 0>
		int32 FindClosestNode(const FVector& Position, EPCGExClusterClosestSearchMode Mode) const
		{
			switch (Mode)
			{
			default: ;
			case EPCGExClusterClosestSearchMode::Vtx:
				return FindClosestNode<MinNeighbors>(Position);
			case EPCGExClusterClosestSearchMode::Edge:
				return FindClosestNodeFromEdge<MinNeighbors>(Position);
			}
		}

		template <int32 MinNeighbors = 0>
		int32 FindClosestNode(const FVector& Position) const
		{
			double MaxDistance = MAX_dbl;
			int32 ClosestIndex = -1;

			const TArray<FNode>& NodesRef = *Nodes;

			if (NodeOctree)
			{
				auto ProcessCandidate = [&](const PCGEx::FIndexedItem& Item)
				{
					const FNode& Node = NodesRef[Item.Index];
					if constexpr (MinNeighbors > 0) { if (Node.Num() < MinNeighbors) { return; } }
					const double Dist = FVector::DistSquared(Position, GetPos(Node));
					if (Dist < MaxDistance)
					{
						MaxDistance = Dist;
						ClosestIndex = Node.Index;
					}
				};

				NodeOctree->FindNearbyElements(Position, ProcessCandidate);
			}
			else
			{
				for (const FNode& Node : (*Nodes))
				{
					if constexpr (MinNeighbors > 0) { if (Node.Num() < MinNeighbors) { continue; } }
					const double Dist = FVector::DistSquared(Position, GetPos(Node));
					if (Dist < MaxDistance)
					{
						MaxDistance = Dist;
						ClosestIndex = Node.Index;
					}
				}
			}

			return ClosestIndex;
		}

		template <int32 MinNeighbors = 0>
		int32 FindClosestNodeFromEdge(const FVector& Position) const
		{
			double MaxDistance = MAX_dbl;
			int32 ClosestIndex = -1;

			if (EdgeOctree)
			{
				auto ProcessCandidate = [&](const PCGEx::FIndexedItem& Item)
				{
					const double Dist = GetPointDistToEdgeSquared(Item.Index, Position);
					if (Dist < MaxDistance)
					{
						if constexpr (MinNeighbors > 0)
						{
							if (GetEdgeStart(Item.Index)->Links.Num() < MinNeighbors &&
								GetEdgeEnd(Item.Index)->Links.Num() < MinNeighbors)
							{
								return;
							}
						}
						MaxDistance = Dist;
						ClosestIndex = Item.Index;
					}
				};

				EdgeOctree->FindNearbyElements(Position, ProcessCandidate);
			}
			else if (BoundedEdges)
			{
				for (const FBoundedEdge& Edge : (*BoundedEdges))
				{
					const double Dist = GetPointDistToEdgeSquared(Edge.Index, Position);
					if (Dist < MaxDistance)
					{
						if constexpr (MinNeighbors > 0)
						{
							if (GetEdgeStart(Edge.Index)->Links.Num() < MinNeighbors &&
								GetEdgeEnd(Edge.Index)->Links.Num() < MinNeighbors)
							{
								continue;
							}
						}

						MaxDistance = Dist;
						ClosestIndex = Edge.Index;
					}
				}
			}
			else
			{
				for (const FEdge& Edge : (*Edges))
				{
					const double Dist = GetPointDistToEdgeSquared(Edge, Position);
					if (Dist < MaxDistance)
					{
						if constexpr (MinNeighbors > 0)
						{
							if (GetEdgeStart(Edge.Index)->Links.Num() < MinNeighbors &&
								GetEdgeEnd(Edge.Index)->Links.Num() < MinNeighbors)
							{
								continue;
							}
						}

						MaxDistance = Dist;
						ClosestIndex = Edge.Index;
					}
				}
			}

			if (ClosestIndex == -1) { return -1; }

			const FEdge* Edge = GetEdge(ClosestIndex);
			const FNode* Start = GetEdgeStart(Edge);
			const FNode* End = GetEdgeEnd(Edge);

			ClosestIndex = FVector::DistSquared(Position, GetPos(Start)) < FVector::DistSquared(Position, GetPos(End)) ? Start->Index : End->Index;

			return ClosestIndex;
		}

		template <int32 MinNeighbors = 0>
		int32 FindClosestEdge(const int32 InNodeIndex, const FVector& InPosition) const
		{
			if (!Nodes->IsValidIndex(InNodeIndex) || (Nodes->GetData() + InNodeIndex)->IsEmpty()) { return -1; }
			const FNode& Node = *(Nodes->GetData() + InNodeIndex);

			double BestDist = MAX_dbl;
			double BestDot = MAX_dbl;

			int32 BestIndex = -1;

			const FVector Position = GetPos(Node);
			const FVector SearchDirection = (GetPos(Node) - InPosition).GetSafeNormal();

			for (const FLink Lk : Node.Links)
			{
				if constexpr (MinNeighbors > 0)
				{
					if (GetNode(Lk)->Links.Num() < MinNeighbors) { continue; }
				}

				FVector NPos = GetPos(Lk.Node);
				const double Dist = FMath::PointDistToSegmentSquared(InPosition, Position, NPos);
				if (Dist <= BestDist)
				{
					const double Dot = FMath::Abs(FVector::DotProduct(SearchDirection, (NPos - Position).GetSafeNormal()));
					if (Dist == BestDist && Dot > BestDot) { continue; }
					BestDot = Dot;
					BestDist = Dist;
					BestIndex = Lk.Edge;
				}
			}

			return BestIndex;
		}

		int32 FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, int32 MinNeighborCount = 1) const;
		int32 FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, const TSet<int32>& Exclusion, int32 MinNeighborCount = 1) const;

		void ComputeEdgeLengths(bool bNormalize = false);

		void GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth) const;
		void GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth, const TSet<int32>& Skip) const;

		void GetConnectedEdges(const int32 FromNodeIndex, TArray<int32>& OutNodeIndices, TArray<int32>& OutEdgeIndices, const int32 SearchDepth) const;
		void GetConnectedEdges(const int32 FromNodeIndex, TArray<int32>& OutNodeIndices, TArray<int32>& OutEdgeIndices, const int32 SearchDepth, const TSet<int32>& SkipNodes, const TSet<int32>& SkipEdges) const;

		FVector GetClosestPointOnEdge(const int32 FromIndex, const int32 ToIndex, const FVector& Position) const;
		FVector GetClosestPointOnEdge(const FEdge& InEdge, const FVector& Position) const;
		FVector GetClosestPointOnEdge(const int32 EdgeIndex, const FVector& Position) const;
		double GetPointDistToEdgeSquared(const FEdge& InEdge, const FVector& Position) const;
		double GetPointDistToEdgeSquared(const int32 EdgeIndex, const FVector& Position) const;

		FVector GetCentroid(const int32 NodeIndex) const;

		void GetValidEdges(TArray<FEdge>& OutValidEdges) const;

		int32 FindClosestNeighborInDirection(const int32 NodeIndex, const FVector& Direction, int32 MinNeighborCount = 1) const;

		TSharedPtr<TArray<FBoundedEdge>> GetBoundedEdges(const bool bBuild);
		void ExpandEdges(PCGExMT::FTaskManager* AsyncManager);

		template <typename T, class MakeFunc>
		void GrabNeighbors(const int32 NodeIndex, TArray<T>& OutNeighbors, const MakeFunc&& Make) const
		{
			FNode* Node = (Nodes->GetData() + NodeIndex);
			PCGEx::InitArray(OutNeighbors, Node->Num());
			for (int i = 0; i < Node->Num(); i++)
			{
				const FLink Lk = Node->Links[i];
				OutNeighbors[i] = Make(Node, (Nodes->GetData() + Lk.Node), (Edges->GetData() + Lk.Edge));
			}
		}

		template <typename T, class MakeFunc>
		void GrabNeighbors(const FNode& Node, TArray<T>& OutNeighbors, const MakeFunc&& Make) const
		{
			PCGEx::InitArray(OutNeighbors, Node.Num());
			for (int i = 0; i < Node.Num(); i++)
			{
				const FLink Lk = Node.Links[i];
				OutNeighbors[i] = Make((Nodes->GetData() + Lk.Node), (Edges->GetData() + Lk.Edge));
			}
		}

	protected:
		int32 GetOrCreateNode_Unsafe(const int32 PointIndex);
		int32 GetOrCreateNode_Unsafe(TSparseArray<int32>& InLookup, const int32 PointIndex);
	};

	void GetAdjacencyData(const FCluster* InCluster, FNode& InNode, TArray<FAdjacencyData>& OutData);
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExEdgeDirectionSettings
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

	TSharedPtr<PCGExSorting::TPointSorter<>> Sorter;

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

	bool SortEndpoints(const PCGExCluster::FCluster* InCluster, PCGExGraph::FEdge& InEdge) const;
	bool SortExtrapolation(const PCGExCluster::FCluster* InCluster, const int32 InEdgeIndex, const int32 StartNodeIndex, const int32 EndNodeIndex) const;
};
