// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExEdge.h"
#include "PCGExGraph.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Geometry/PCGExGeo.h"

#include "PCGExCluster.generated.h"

namespace PCGExCluster
{
	struct FAdjacencyData;
	struct FExpandedNode;
	struct FExpandedEdge;
}

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Cluster Closest Search Mode"))
enum class EPCGExClusterClosestSearchMode : uint8
{
	Node UMETA(DisplayName = "Closest node", ToolTip="Proximity to node position"),
	Edge UMETA(DisplayName = "Closest edge", ToolTip="Proximity to edge, then endpoint"),
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

	PCGEX_ASYNC_STATE(State_ProcessingCluster)

	struct PCGEXTENDEDTOOLKIT_API FClusterItemRef
	{
		int32 ItemIndex;
		FBoxSphereBounds Bounds;

		FClusterItemRef(const int32 InItemIndex, const FBoxSphereBounds& InBounds)
			: ItemIndex(InItemIndex), Bounds(InBounds)
		{
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FClusterItemRefSemantics
	{
		enum { MaxElementsPerLeaf = 16 };

		enum { MinInclusiveElementsPerNode = 7 };

		enum { MaxNodeDepth = 12 };

		using ElementAllocator = TInlineAllocator<MaxElementsPerLeaf>;

		FORCEINLINE static const FBoxSphereBounds& GetBoundingBox(const FClusterItemRef& InNode)
		{
			return InNode.Bounds;
		}

		FORCEINLINE static const bool AreElementsEqual(const FClusterItemRef& A, const FClusterItemRef& B)
		{
			return A.ItemIndex == B.ItemIndex;
		}

		FORCEINLINE static void ApplyOffset(FClusterItemRef& InNode)
		{
			ensureMsgf(false, TEXT("Not implemented"));
		}

		FORCEINLINE static void SetElementId(const FClusterItemRef& Element, FOctreeElementId2 OctreeElementID)
		{
		}
	};

	struct FCluster;

	struct PCGEXTENDEDTOOLKIT_API FNode : public PCGExGraph::FNode
	{
		FVector Position;

		FNode(): PCGExGraph::FNode()
		{
			Position = FVector::ZeroVector;
		}

		FNode(const int32 InNodeIndex, const int32 InPointIndex, const FVector& InPosition):
			PCGExGraph::FNode(InNodeIndex, InPointIndex), Position(InPosition)
		{
		}

		~FNode();

		FORCEINLINE bool IsDeadEnd() const { return Adjacency.Num() == 1; }
		FORCEINLINE bool IsSimple() const { return Adjacency.Num() == 2; }
		FORCEINLINE bool IsComplex() const { return Adjacency.Num() > 2; }

		FORCEINLINE bool IsAdjacentTo(const int32 OtherNodeIndex) const
		{
			for (const uint64 AdjacencyHash : Adjacency) { if (OtherNodeIndex == PCGEx::H64A(AdjacencyHash)) { return true; } }
			return false;
		}

		FORCEINLINE void AddConnection(const int32 InNodeIndex, const int32 InEdgeIndex)
		{
			Adjacency.AddUnique(PCGEx::H64(InNodeIndex, InEdgeIndex));
		}

		FVector GetCentroid(const FCluster* InCluster) const;

		FORCEINLINE int32 GetEdgeIndex(int32 AdjacentNodeIndex) const
		{
			for (const uint64 AdjacencyHash : Adjacency) { if (PCGEx::H64A(AdjacencyHash) == AdjacentNodeIndex) { return PCGEx::H64B(AdjacencyHash); } }
			return -1;
		}

		void ComputeNormal(const FCluster* InCluster, const TArray<FAdjacencyData>& AdjacencyData, FVector& OutNormal) const;

		FORCEINLINE void Add(const FNode& Neighbor, int32 EdgeIndex) { Adjacency.Add(PCGEx::H64(Neighbor.NodeIndex, EdgeIndex)); }
	};

	struct PCGEXTENDEDTOOLKIT_API FExpandedNeighbor
	{
		const FNode* Node;
		const PCGExGraph::FIndexedEdge* Edge;
		FVector Direction;

		FExpandedNeighbor(const FNode* InNode, const PCGExGraph::FIndexedEdge* InEdge, const FVector& InDirection):
			Node(InNode), Edge(InEdge), Direction(InDirection)
		{
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FCluster
	{
	protected:
		bool bIsMirror = false;
		bool bOwnsNodes = true;
		bool bOwnsEdges = true;
		bool bOwnsNodeIndexLookup = true;
		bool bOwnsNodeOctree = true;
		bool bOwnsEdgeOctree = true;
		bool bOwnsLengths = true;
		bool bOwnsVtxPointIndices = true;
		bool bOwnsExpandedNodes = true;
		bool bOwnsExpandedEdges = true;

		bool bEdgeLengthsDirty = true;
		bool bIsCopyCluster = false;
		TArray<int32>* VtxPointIndices = nullptr;
		TArray<uint64>* VtxPointScopes = nullptr;

		mutable FRWLock ClusterLock;

	public:
		int32 NumRawVtx = 0;
		int32 NumRawEdges = 0;

		bool bValid = false;
		bool bIsOneToOne = false; // Whether the input data has a single set of edges for a single set of vtx

		int32 ClusterID = -1;
		TMap<int32, int32>* NodeIndexLookup = nullptr; // Node index -> Point Index
		//TMap<uint64, int32> EdgeIndexLookup;   // Edge Hash -> Edge Index
		TArray<FNode>* Nodes = nullptr;
		TArray<FExpandedNode*>* ExpandedNodes = nullptr;
		TArray<FExpandedEdge*>* ExpandedEdges = nullptr;
		TArray<PCGExGraph::FIndexedEdge>* Edges = nullptr;
		TArray<double>* EdgeLengths = nullptr;

		FBox Bounds;

		PCGExData::FPointIO* VtxIO = nullptr;
		PCGExData::FPointIO* EdgesIO = nullptr;

		using ClusterItemOctree = TOctree2<FClusterItemRef, FClusterItemRefSemantics>;
		ClusterItemOctree* NodeOctree = nullptr;
		ClusterItemOctree* EdgeOctree = nullptr;

		FCluster();
		FCluster(const FCluster* OtherCluster, PCGExData::FPointIO* InVtxIO, PCGExData::FPointIO* InEdgesIO,
		         bool bCopyNodes, bool bCopyEdges, bool bCopyLookup);

		void ClearInheritedForChanges(const bool bClearOwned = false);
		void WillModifyVtxIO(const bool bClearOwned = false);
		void WillModifyVtxPositions(const bool bClearOwned = false);

		~FCluster();

		bool BuildFrom(
			const PCGExData::FPointIO* EdgeIO,
			const TArray<FPCGPoint>& InNodePoints,
			const TMap<uint32, int32>& InEndpointsLookup,
			const TArray<int32>* InExpectedAdjacency = nullptr);

		void BuildFrom(const PCGExGraph::FSubGraph* SubGraph);

		bool IsValidWith(const PCGExData::FPointIO* InVtxIO, const PCGExData::FPointIO* InEdgesIO) const;

		const TArray<uint64>* GetVtxPointScopesPtr();
		const TArray<int32>& GetVtxPointIndices();
		TArrayView<const int32> GetVtxPointIndicesView();

		const TArray<int32>* GetVtxPointIndicesPtr();
		const TArray<uint64>& GetVtxPointScopes();
		TArrayView<const uint64> GetVtxPointScopesView();
		
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

		FORCEINLINE FVector GetEdgeDirection(const int32 FromIndex, const int32 ToIndex) const
		{
			return ((Nodes->GetData() + FromIndex)->Position - (Nodes->GetData() + ToIndex)->Position).GetSafeNormal();
		}

		FORCEINLINE FVector GetCentroid(const int32 NodeIndex) const
		{
			const FNode* Node = (Nodes->GetData() + NodeIndex);
			FVector Centroid = FVector::ZeroVector;
			for (const uint64 AdjacencyHash : Node->Adjacency) { Centroid += (Nodes->GetData() + PCGEx::H64A(AdjacencyHash))->Position; }
			return Centroid / static_cast<double>(Node->Adjacency.Num());
		}

		void GetValidEdges(TArray<PCGExGraph::FIndexedEdge>& OutValidEdges) const;

		int32 FindClosestNeighborInDirection(const int32 NodeIndex, const FVector& Direction, int32 MinNeighborCount = 1) const;

		TArray<FExpandedNode*>* GetExpandedNodes(const bool bBuild);
		void ExpandNodes(PCGExMT::FTaskManager* AsyncManager);

		TArray<FExpandedEdge*>* GetExpandedEdges(const bool bBuild);
		void ExpandEdges(PCGExMT::FTaskManager* AsyncManager);

		template <typename T, class MakeFunc>
		void GrabNeighbors(const int32 NodeIndex, TArray<T>& OutNeighbors, const MakeFunc&& Make) const
		{
			FNode* Node = (Nodes->GetData() + NodeIndex);
			PCGEX_SET_NUM_UNINITIALIZED(OutNeighbors, Node->Adjacency.Num())
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
			PCGEX_SET_NUM_UNINITIALIZED(OutNeighbors, Node.Adjacency.Num())
			for (int i = 0; i < Node.Adjacency.Num(); i++)
			{
				uint32 OtherNodeIndex;
				uint32 EdgeIndex;
				PCGEx::H64(Node.Adjacency[i], OtherNodeIndex, EdgeIndex);
				OutNeighbors[i] = Make((Nodes->GetData() + OtherNodeIndex), (Edges->GetData() + EdgeIndex));
			}
		}

	protected:
		FORCEINLINE FNode& GetOrCreateNodeUnsafe(const TArray<FPCGPoint>& InNodePoints, int32 PointIndex)
		{
			const int32* NodeIndex = NodeIndexLookup->Find(PointIndex);

			if (!NodeIndex)
			{
				NodeIndexLookup->Add(PointIndex, Nodes->Num());
				FNode& NewNode = Nodes->Emplace_GetRef(Nodes->Num(), PointIndex, InNodePoints[PointIndex].Transform.GetLocation());
				Bounds += NewNode.Position;
				return NewNode;
			}

			return *(Nodes->GetData() + *NodeIndex);
		}

		void CreateVtxPointIndices();
		void CreateVtxPointScopes();
	};

	struct PCGEXTENDEDTOOLKIT_API FExpandedNode
	{
		const FNode* Node = nullptr;
		TArray<FExpandedNeighbor> Neighbors;

		FExpandedNode(const FCluster* Cluster, const int32 InNodeIndex):
			Node(Cluster->Nodes->GetData() + InNodeIndex)
		{
			const int32 NumNeighbors = Node->Adjacency.Num();
			PCGEX_SET_NUM_UNINITIALIZED(Neighbors, NumNeighbors)
			for (int i = 0; i < Neighbors.Num(); i++)
			{
				uint32 NodeIndex;
				uint32 EdgeIndex;
				PCGEx::H64(Node->Adjacency[i], NodeIndex, EdgeIndex);
				Neighbors[i] = FExpandedNeighbor(
					Cluster->Nodes->GetData() + NodeIndex, Cluster->Edges->GetData() + EdgeIndex,
					((Cluster->Nodes->GetData() + NodeIndex)->Position - Node->Position).GetSafeNormal());
			}
		}

		~FExpandedNode()
		{
			Node = nullptr;
			Neighbors.Empty();
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FExpandedEdge
	{
		const int32 Index;
		const FNode* Start = nullptr;
		const FNode* End = nullptr;
		const FBoxSphereBounds Bounds;

		FExpandedEdge(const FCluster* Cluster, const int32 InEdgeIndex):
			Index(InEdgeIndex),
			Start(Cluster->Nodes->GetData() + (*Cluster->NodeIndexLookup)[(Cluster->Edges->GetData() + InEdgeIndex)->Start]),
			End(Cluster->Nodes->GetData() + (*Cluster->NodeIndexLookup)[(Cluster->Edges->GetData() + InEdgeIndex)->End]),
			Bounds(FBoxSphereBounds(FSphere(FMath::Lerp(Start->Position, End->Position, 0.5), FVector::Dist(Start->Position, End->Position) * 0.5)))
		{
		}

		~FExpandedEdge()
		{
			Start = nullptr;
			End = nullptr;
		}

		FORCEINLINE double GetEdgeLength() const { return FVector::Dist(Start->Position, End->Position); }
		FORCEINLINE double GetEdgeLengthSquared() const { return FVector::DistSquared(Start->Position, End->Position); }
		FORCEINLINE FVector GetCenter() const { return Bounds.Origin; }
		FORCEINLINE int32 OtherNodeIndex(int32 NodeIndex) const
		{
			check(NodeIndex == Start->NodeIndex || NodeIndex == End->NodeIndex)
			return NodeIndex == Start->NodeIndex ? End->NodeIndex : Start->NodeIndex;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FNodeChain
	{
		int32 First = -1;
		int32 Last = -1;
		int32 SingleEdge = -1;
		TArray<int32> Nodes;
		TArray<int32> Edges;

		FNodeChain()
		{
		}

		~FNodeChain()
		{
			Nodes.Empty();
			Edges.Empty();
		}

		uint64 GetUniqueHash() const
		{
			const uint32 BaseHash = First > Last ? HashCombineFast(GetTypeHash(Last), GetTypeHash(First)) : HashCombineFast(GetTypeHash(First), GetTypeHash(Last));
			if (SingleEdge != -1) { return BaseHash; }
			if (Nodes.Num() == 1) { return HashCombineFast(BaseHash, GetTypeHash(Nodes[0])); }
			const uint32 NodeBaseHash = Nodes[0] > Nodes[1] ? HashCombineFast(GetTypeHash(Nodes[1]), GetTypeHash(Nodes[0])) : HashCombineFast(GetTypeHash(Nodes[0]), GetTypeHash(Nodes[1]));
			return HashCombineFast(BaseHash, NodeBaseHash);
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FAdjacencyData
	{
		int32 NodeIndex = -1;
		int32 NodePointIndex = -1;
		int32 EdgeIndex = -1;
		FVector Direction = FVector::OneVector;
		double Length = 0;
	};

	static void GetAdjacencyData(FCluster* InCluster, FNode& InNode, TArray<FAdjacencyData>& OutData)
	{
		const int32 NumAdjacency = InNode.Adjacency.Num();
		const FVector NodePosition = InNode.Position;
		OutData.Reserve(NumAdjacency);
		for (int i = 0; i < NumAdjacency; i++)
		{
			uint32 NIndex;
			uint32 EIndex;

			PCGEx::H64(InNode.Adjacency[i], NIndex, EIndex);

			const FNode* OtherNode = InCluster->Nodes->GetData() + NIndex;
			const FVector OtherPosition = OtherNode->Position;

			FAdjacencyData& Data = OutData.Emplace_GetRef();
			Data.NodeIndex = NIndex;
			Data.NodePointIndex = OtherNode->PointIndex;
			Data.EdgeIndex = EIndex;
			Data.Direction = (NodePosition - OtherPosition).GetSafeNormal();
			Data.Length = FVector::Dist(NodePosition, OtherPosition);
		}
	}
}

namespace PCGExClusterTask
{
	class PCGEXTENDEDTOOLKIT_API FBuildCluster final : public PCGExMT::FPCGExTask
	{
	public:
		FBuildCluster(
			PCGExData::FPointIO* InPointIO,
			PCGExCluster::FCluster* InCluster,
			const PCGExData::FPointIO* InEdgeIO,
			const TMap<uint32, int32>* InEndpointsLookup,
			const TArray<int32>* InExpectedAdjacency) :
			FPCGExTask(InPointIO),
			Cluster(InCluster),
			EdgeIO(InEdgeIO),
			EndpointsLookup(InEndpointsLookup),
			ExpectedAdjacency(InExpectedAdjacency)
		{
		}

		PCGExCluster::FCluster* Cluster = nullptr;
		const PCGExData::FPointIO* EdgeIO = nullptr;
		const TMap<uint32, int32>* EndpointsLookup = nullptr;
		const TArray<int32>* ExpectedAdjacency = nullptr;

		virtual bool ExecuteTask() override;
	};


	class PCGEXTENDEDTOOLKIT_API FFindNodeChains final : public PCGExMT::FPCGExTask
	{
	public:
		FFindNodeChains(PCGExData::FPointIO* InPointIO,
		                const PCGExCluster::FCluster* InCluster,
		                const TArray<bool>* InBreakpoints,
		                TArray<PCGExCluster::FNodeChain*>* InChains,
		                const bool InSkipSingleEdgeChains = false,
		                const bool InDeadEndsOnly = false) :
			FPCGExTask(InPointIO),
			Cluster(InCluster),
			Breakpoints(InBreakpoints),
			Chains(InChains),
			bSkipSingleEdgeChains(InSkipSingleEdgeChains),
			bDeadEndsOnly(InDeadEndsOnly)
		{
		}

		const PCGExCluster::FCluster* Cluster = nullptr;
		const TArray<bool>* Breakpoints = nullptr;
		TArray<PCGExCluster::FNodeChain*>* Chains = nullptr;

		const bool bSkipSingleEdgeChains = false;
		const bool bDeadEndsOnly = false;

		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FBuildChain final : public PCGExMT::FPCGExTask
	{
	public:
		FBuildChain(PCGExData::FPointIO* InPointIO,
		            const PCGExCluster::FCluster* InCluster,
		            const TArray<bool>* InBreakpoints,
		            TArray<PCGExCluster::FNodeChain*>* InChains,
		            const int32 InStartIndex,
		            const uint64 InAdjacencyHash) :
			FPCGExTask(InPointIO),
			Cluster(InCluster),
			Breakpoints(InBreakpoints),
			Chains(InChains),
			StartIndex(InStartIndex),
			AdjacencyHash(InAdjacencyHash)
		{
		}

		const PCGExCluster::FCluster* Cluster = nullptr;
		const TArray<bool>* Breakpoints = nullptr;
		TArray<PCGExCluster::FNodeChain*>* Chains = nullptr;
		int32 StartIndex = 0;
		uint64 AdjacencyHash = 0;

		virtual bool ExecuteTask() override;
	};

	static void BuildChain(
		PCGExCluster::FNodeChain* Chain,
		const TArray<bool>* Breakpoints,
		const PCGExCluster::FCluster* Cluster)
	{
		TArray<PCGExCluster::FNode>& Nodes = *Cluster->Nodes;

		int32 LastIndex = Chain->First;
		int32 NextIndex = Chain->Last;
		Chain->Edges.Add(Nodes[LastIndex].GetEdgeIndex(NextIndex));

		while (NextIndex != -1)
		{
			const PCGExCluster::FNode& NextNode = Nodes[NextIndex];
			if ((*(Breakpoints->GetData() + NextIndex)) || NextNode.IsComplex() || NextNode.IsDeadEnd())
			{
				LastIndex = NextIndex;
				break;
			}

			uint32 OtherIndex;
			uint32 EdgeIndex;
			PCGEx::H64(NextNode.Adjacency[0], OtherIndex, EdgeIndex);                                  // Get next node
			if (OtherIndex == LastIndex) { PCGEx::H64(NextNode.Adjacency[1], OtherIndex, EdgeIndex); } // Get other next

			LastIndex = NextIndex;
			NextIndex = OtherIndex;

			Chain->Nodes.Add(LastIndex);
			Chain->Edges.Add(EdgeIndex);
		}

		Chain->Last = LastIndex;
	}

	static void DedupeChains(TArray<PCGExCluster::FNodeChain*>& InChains)
	{
		TSet<uint64> Chains;
		Chains.Reserve(InChains.Num() / 2);


		for (int i = 0; i < InChains.Num(); i++)
		{
			const PCGExCluster::FNodeChain* Chain = InChains[i];
			if (!Chain) { continue; }

			bool bAlreadyExists = false;
			Chains.Add(Chain->GetUniqueHash(), &bAlreadyExists);
			if (bAlreadyExists)
			{
				PCGEX_DELETE(Chain)
				InChains[i] = nullptr;
			}
		}
	}

	class PCGEXTENDEDTOOLKIT_API FCopyClustersToPoint final : public PCGExMT::FPCGExTask
	{
	public:
		FCopyClustersToPoint(PCGExData::FPointIO* InPointIO,
		                     PCGExData::FPointIO* InVtx,
		                     const TArray<PCGExData::FPointIO*>& InEdges,
		                     PCGExData::FPointIOCollection* InVtxCollection,
		                     PCGExData::FPointIOCollection* InEdgeCollection,
		                     FPCGExTransformDetails* InTransformDetails) :
			FPCGExTask(InPointIO),
			Vtx(InVtx),
			Edges(InEdges),
			VtxCollection(InVtxCollection),
			EdgeCollection(InEdgeCollection),
			TransformDetails(InTransformDetails)
		{
		}

		virtual ~FCopyClustersToPoint() override;

		PCGExData::FPointIO* Vtx = nullptr;
		TArray<PCGExData::FPointIO*> Edges;

		PCGExData::FPointIOCollection* VtxCollection = nullptr;
		PCGExData::FPointIOCollection* EdgeCollection = nullptr;

		FPCGExTransformDetails* TransformDetails = nullptr;

		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FTransformCluster final : public PCGExMT::FPCGExTask
	{
	public:
		FTransformCluster(
			PCGExData::FPointIO* InPointIO,
			PCGExCluster::FCluster* InCluster,
			FPCGExTransformDetails* InTransformDetails) :
			FPCGExTask(InPointIO),
			Cluster(InCluster),
			TransformDetails(InTransformDetails)
		{
		}

		PCGExCluster::FCluster* Cluster = nullptr;
		FPCGExTransformDetails* TransformDetails = nullptr;

		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FExpandClusterNodes final : public PCGExMT::FPCGExTask
	{
	public:
		FExpandClusterNodes(PCGExData::FPointIO* InPointIO, PCGExCluster::FCluster* InCluster, const int32 InNumIterations) :
			FPCGExTask(InPointIO), Cluster(InCluster), NumIterations(InNumIterations)
		{
		}

		PCGExCluster::FCluster* Cluster = nullptr;
		int32 NumIterations = 0;

		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FExpandClusterEdges final : public PCGExMT::FPCGExTask
	{
	public:
		FExpandClusterEdges(PCGExData::FPointIO* InPointIO, PCGExCluster::FCluster* InCluster, const int32 InNumIterations) :
			FPCGExTask(InPointIO), Cluster(InCluster), NumIterations(InNumIterations)
		{
		}

		PCGExCluster::FCluster* Cluster = nullptr;
		int32 NumIterations = 0;

		virtual bool ExecuteTask() override;
	};
}
