// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExEdge.h"
#include "PCGExGraph.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExDataState.h"
#include "Data/PCGExDataFilter.h"
#include "Geometry/PCGExGeo.h"

#include "PCGExCluster.generated.h"


UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Cluster Closest Search Mode"))
enum class EPCGExClusterClosestSearchMode : uint8
{
	Node UMETA(DisplayName = "Closest node", ToolTip="Proximity to node position"),
	Edge UMETA(DisplayName = "Closest edge", ToolTip="Proximity to edge, then endpoint"),
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExNodeSelectionSettings
{
	GENERATED_BODY()

	FPCGExNodeSelectionSettings()
	{
	}

	explicit FPCGExNodeSelectionSettings(const double InMaxDistance):
		MaxDistance(InMaxDistance)
	{
	}

	~FPCGExNodeSelectionSettings()
	{
	}

	/** Drives how the seed & goal points are selected within each cluster. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExClusterClosestSearchMode PickingMethod = EPCGExClusterClosestSearchMode::Edge;

	/** Max distance at which a node can be selected. Use <= 0 to ignore distance check. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double MaxDistance = -1;

	FORCEINLINE bool WithinDistance(const FVector& NodePosition, const FVector& TargetPosition) const
	{
		if (MaxDistance <= 0) { return true; }
		return FVector::Distance(NodePosition, TargetPosition) < MaxDistance;
	}
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] CLuster Search Orientation Mode"))
enum class EPCGExClusterSearchOrientationMode : uint8
{
	CCW UMETA(DisplayName = "Counter Clockwise"),
	CW UMETA(DisplayName = "Clockwise"),
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExClusterFilterFactoryBase : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override;
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExNodeStateFactory : public UPCGExDataStateFactoryBase
{
	GENERATED_BODY()

public:
	TArray<UPCGExFilterFactoryBase*> FilterFactories;
	virtual PCGExFactories::EType GetFactoryType() const override;
	virtual PCGExDataFilter::TFilter* CreateFilter() const override;
	virtual void BeginDestroy() override;
};


namespace PCGExCluster
{
	const FName OutputNodeStateLabel = TEXT("NodeState");
	const FName SourceNodeStateLabel = TEXT("NodeStates");

	PCGEX_ASYNC_STATE(State_ProcessingCluster)
	PCGEX_ASYNC_STATE(State_ProjectingCluster)

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

		FNode():
			PCGExGraph::FNode()
		{
			Position = FVector::ZeroVector;
		}

		FNode(const int32 InNodeIndex, const int32 InPointIndex, const FVector& InPosition):
			PCGExGraph::FNode(InNodeIndex, InPointIndex), Position(InPosition)
		{
		}

		~FNode();

		FORCEINLINE bool IsDeadEnd() const;
		FORCEINLINE bool IsSimple() const;
		FORCEINLINE bool IsComplex() const;

		FORCEINLINE bool IsAdjacentTo(const int32 OtherNodeIndex) const;

		FORCEINLINE void AddConnection(const int32 InNodeIndex, const int32 InEdgeIndex);
		FORCEINLINE FVector GetCentroid(FCluster* InCluster) const;
		FORCEINLINE int32 GetEdgeIndex(int32 AdjacentNodeIndex) const;

		void ExtractAdjacencies(TArray<int32>& OutNodes, TArray<int32>& OutEdges) const;
		FORCEINLINE void Add(const FNode& Neighbor, int32 Edge);
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

		void RebuildBounds();
		void RebuildNodeOctree();
		void RebuildEdgeOctree();
		void RebuildOctree(EPCGExClusterClosestSearchMode Mode);

		int32 FindClosestNode(const FVector& Position, EPCGExClusterClosestSearchMode Mode, const int32 MinNeighbors = 0) const;
		int32 FindClosestNode(const FVector& Position, const int32 MinNeighbors = 0) const;
		int32 FindClosestNodeFromEdge(const FVector& Position, const int32 MinNeighbors = 0) const;

		int32 FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, int32 MinNeighborCount = 1) const;
		int32 FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, const TSet<int32>& Exclusion, int32 MinNeighborCount = 1) const;

		void ComputeEdgeLengths(bool bNormalize = false);

		FORCEINLINE void GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth) const;
		FORCEINLINE void GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth, const TSet<int32>& Skip) const;

		FORCEINLINE void GetConnectedEdges(const int32 FromNodeIndex, TArray<int32>& OutNodeIndices, TArray<int32>& OutEdgeIndices, const int32 SearchDepth) const;
		FORCEINLINE void GetConnectedEdges(const int32 FromNodeIndex, TArray<int32>& OutNodeIndices, TArray<int32>& OutEdgeIndices, const int32 SearchDepth, const TSet<int32>& SkipNodes, const TSet<int32>& SkipEdges) const;

		FORCEINLINE FVector GetEdgeDirection(const int32 FromIndex, const int32 ToIndex) const;
		FORCEINLINE FVector GetCentroid(const int32 NodeIndex) const;

		void GetValidEdges(TArray<PCGExGraph::FIndexedEdge>& OutValidEdges) const;

		int32 FindClosestNeighborInDirection(const int32 NodeIndex, const FVector& Direction, int32 MinNeighborCount = 1) const;

	protected:
		FORCEINLINE FNode& GetOrCreateNodeUnsafe(const TArray<FPCGPoint>& InNodePoints, int32 PointIndex);
		void CreateVtxPointIndices();
		void CreateVtxPointScopes();
	};

	struct PCGEXTENDEDTOOLKIT_API FNodeProjection
	{
		FNode* Node = nullptr;
		FVector Normal = FVector::UpVector;
		TArray<uint64> SortedAdjacency;

		explicit FNodeProjection(FNode* InNode);

		void Project(const FCluster* InCluster, const FPCGExGeo2DProjectionSettings* ProjectionSettings);
		void ComputeNormal(const FCluster* InCluster);
		FORCEINLINE int32 GetAdjacencyIndex(int32 NodeIndex) const;

		~FNodeProjection();
	};

	struct PCGEXTENDEDTOOLKIT_API FClusterProjection
	{
		FCluster* Cluster = nullptr;
		FPCGExGeo2DProjectionSettings* ProjectionSettings = nullptr;
		TArray<FNodeProjection> Nodes;

		FClusterProjection(FCluster* InCluster, FPCGExGeo2DProjectionSettings* InProjectionSettings);

		~FClusterProjection();

		void Build();
		FORCEINLINE int32 FindNextAdjacentNode(EPCGExClusterSearchOrientationMode Orient, int32 NodeIndex, int32 From, const TSet<int32>& Exclusion, const int32 MinNeighbors);
		FORCEINLINE int32 FindNextAdjacentNodeCCW(int32 NodeIndex, int32 From, const TSet<int32>& Exclusion, const int32 MinNeighbors);
		FORCEINLINE int32 FindNextAdjacentNodeCW(int32 NodeIndex, int32 From, const TSet<int32>& Exclusion, const int32 MinNeighbors);
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

		uint64 GetNHash() const { return PCGEx::NH64(First, Last); }
		uint64 GetNHashU() const { return PCGEx::NH64U(First, Last); }
	};

	class PCGEXTENDEDTOOLKIT_API TClusterNodeFilter : public PCGExDataFilter::TFilter
	{
	public:
		explicit TClusterNodeFilter(const UPCGExClusterFilterFactoryBase* InFactory)
			: TFilter(InFactory)
		{
			bValid = false;
		}

		PCGExDataCaching::FPool* EdgeDataCache = nullptr;
		const FCluster* CapturedCluster = nullptr;

		FORCEINLINE virtual PCGExDataFilter::EType GetFilterType() const override;

		virtual void CaptureCluster(const FPCGContext* InContext, const FCluster* InCluster, PCGExDataCaching::FPool* InVtxDataCache, PCGExDataCaching::FPool* InEdgeDataCache);
		virtual void CaptureEdges(const FPCGContext* InContext, const PCGExData::FPointIO* EdgeIO);
		virtual void PrepareForTesting() override;
		virtual void PrepareForTesting(const TArrayView<const int32>& PointIndices) override;
	};

	class PCGEXTENDEDTOOLKIT_API FNodeStateHandler final : public PCGExDataState::TDataState
	{
	public:
		explicit FNodeStateHandler(const UPCGExNodeStateFactory* InFactory);

		const UPCGExNodeStateFactory* NodeStateDefinition = nullptr;

		PCGExDataCaching::FPool* EdgeDataCache = nullptr;

		TArray<TFilter*> FilterHandlers;
		TArray<TClusterNodeFilter*> ClusterFilterHandlers;

		void CaptureCluster(const FPCGContext* InContext, FCluster* InCluster, PCGExDataCaching::FPool* InVtxDataCache, PCGExDataCaching::FPool* InEdgeDataCache);
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override;

		virtual void PrepareForTesting() override;
		virtual void PrepareForTesting(const TArrayView<const int32>& PointIndices) override;

		virtual ~FNodeStateHandler() override
		{
			PCGEX_DELETE_TARRAY(FilterHandlers)
			PCGEX_DELETE_TARRAY(ClusterFilterHandlers)
		}

	protected:
		PCGExData::FPointIO* LastPoints = nullptr;
		FCluster* Cluster = nullptr;
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

			const FNode& OtherNode = (*InCluster->Nodes)[NIndex];
			const FVector OtherPosition = OtherNode.Position;

			FAdjacencyData& Data = OutData.Emplace_GetRef();
			Data.NodeIndex = NIndex;
			Data.NodePointIndex = OtherNode.PointIndex;
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
			if ((*Breakpoints)[NextIndex] || NextNode.IsComplex() || NextNode.IsDeadEnd())
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
			Chains.Add(PCGEx::H64U(Chain->First, Chain->Last), &bAlreadyExists);
			if (bAlreadyExists)
			{
				PCGEX_DELETE(Chain)
				InChains[i] = nullptr;
			}
		}
	}

	class PCGEXTENDEDTOOLKIT_API FProjectCluster final : public PCGExMT::FPCGExTask
	{
	public:
		FProjectCluster(PCGExData::FPointIO* InPointIO,
		                PCGExCluster::FCluster* InCluster, PCGExCluster::FClusterProjection* InProjection) :
			FPCGExTask(InPointIO),
			Cluster(InCluster),
			Projection(InProjection)
		{
		}

		PCGExCluster::FCluster* Cluster = nullptr;
		PCGExCluster::FClusterProjection* Projection = nullptr;

		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FCopyClustersToPoint final : public PCGExMT::FPCGExTask
	{
	public:
		FCopyClustersToPoint(PCGExData::FPointIO* InPointIO,
		                     PCGExData::FPointIO* InVtx,
		                     const TArray<PCGExData::FPointIO*>& InEdges,
		                     PCGExData::FPointIOCollection* InVtxCollection,
		                     PCGExData::FPointIOCollection* InEdgeCollection,
		                     FPCGExTransformSettings* InTransformSettings) :
			FPCGExTask(InPointIO),
			Vtx(InVtx),
			Edges(InEdges),
			VtxCollection(InVtxCollection),
			EdgeCollection(InEdgeCollection),
			TransformSettings(InTransformSettings)
		{
		}

		virtual ~FCopyClustersToPoint() override;

		PCGExData::FPointIO* Vtx = nullptr;
		TArray<PCGExData::FPointIO*> Edges;

		PCGExData::FPointIOCollection* VtxCollection = nullptr;
		PCGExData::FPointIOCollection* EdgeCollection = nullptr;

		FPCGExTransformSettings* TransformSettings = nullptr;

		virtual bool ExecuteTask() override;
	};
}
