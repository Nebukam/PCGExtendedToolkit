// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExEdge.h"
#include "PCGExGraph.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExGraphDefinition.h"
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
	EPCGExClusterClosestSearchMode PickingMethod = EPCGExClusterClosestSearchMode::Node;

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

	constexpr PCGExMT::AsyncState State_ProcessingCluster = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProjectingCluster = __COUNTER__;

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

		TArray<int32> AdjacentNodes;

		FNode()
		{
			PointIndex = -1;
			Position = FVector::ZeroVector;
			AdjacentNodes.Empty();
			Edges.Empty();
		}

		~FNode();

		FORCEINLINE void AddConnection(const int32 InEdgeIndex, const int32 InNodeIndex);
		FORCEINLINE FVector GetCentroid(FCluster* InCluster) const;
		FORCEINLINE int32 GetEdgeIndex(int32 AdjacentNodeIndex) const;
	};

	struct PCGEXTENDEDTOOLKIT_API FCluster
	{
		bool bEdgeLengthsDirty = true;
		bool bValid = false;
		int32 ClusterID = -1;
		TMap<int32, int32> PointIndexMap; // Node index -> Point Index
		TMap<uint64, int32> EdgeIndexMap; // Edge Hash -> Edge Index
		TArray<FNode> Nodes;
		TArray<PCGExGraph::FIndexedEdge> Edges;
		TArray<double> EdgeLengths;
		FBox Bounds;

		PCGExData::FPointIO* PointsIO = nullptr;
		PCGExData::FPointIO* EdgesIO = nullptr;

		using ClusterItemOctree = TOctree2<FClusterItemRef, FClusterItemRefSemantics>;
		ClusterItemOctree* NodeOctree = nullptr;
		ClusterItemOctree* EdgeOctree = nullptr;

		FCluster();

		~FCluster();

		bool BuildFrom(
			const PCGExData::FPointIO& EdgeIO,
			const TArray<FPCGPoint>& InNodePoints,
			const TMap<int64, int32>& InNodeIndicesMap,
			const TArray<int32>& PerNodeEdgeNums, const bool bDeterministic);

		void BuildPartialFrom(const TArray<FVector>& Positions, const TSet<uint64>& InEdges);

		void RebuildNodeOctree();
		void RebuildEdgeOctree();
		void RebuildOctree(EPCGExClusterClosestSearchMode Mode);

		int32 FindClosestNode(const FVector& Position, EPCGExClusterClosestSearchMode Mode, const int32 MinNeighbors = 0) const;
		int32 FindClosestNode(const FVector& Position, const int32 MinNeighbors = 0) const;
		int32 FindClosestNodeFromEdge(const FVector& Position, const int32 MinNeighbors = 0) const;

		int32 FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, int32 MinNeighborCount = 1) const;
		int32 FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, const TSet<int32>& Exclusion, int32 MinNeighborCount = 1) const;

		FORCEINLINE const FNode& GetNodeFromPointIndex(const int32 Index) const;
		FORCEINLINE const PCGExGraph::FIndexedEdge& GetEdgeFromNodeIndices(const int32 A, const int32 B) const;
		void ComputeEdgeLengths(bool bNormalize = false);

		void GetNodePointIndices(TArray<int32>& OutIndices);
		FORCEINLINE void GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth) const;
		FORCEINLINE void GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth, const TSet<int32>& Skip) const;

		FORCEINLINE void GetConnectedEdges(const int32 FromNodeIndex, TArray<int32>& OutNodeIndices, TArray<int32>& OutEdgeIndices, const int32 SearchDepth) const;
		FORCEINLINE void GetConnectedEdges(const int32 FromNodeIndex, TArray<int32>& OutNodeIndices, TArray<int32>& OutEdgeIndices, const int32 SearchDepth, const TSet<int32>& SkipNodes, const TSet<int32>& SkipEdges) const;

		FORCEINLINE FVector GetEdgeDirection(const int32 FromIndex, const int32 ToIndex) const;
		FORCEINLINE FVector GetCentroid(const int32 NodeIndex) const;

		int32 FindClosestNeighborInDirection(const int32 NodeIndex, const FVector& Direction, int32 MinNeighborCount = 1) const;

	protected:
		FORCEINLINE FNode& GetOrCreateNode(const int32 PointIndex, const TArray<FPCGPoint>& InPoints);
	};

	struct PCGEXTENDEDTOOLKIT_API FNodeProjection
	{
		FNode* Node = nullptr;
		FVector Normal = FVector::UpVector;
		TArray<int32> SortedAdjacency;

		explicit FNodeProjection(FNode* InNode);

		void Project(FCluster* InCluster, const FPCGExGeo2DProjectionSettings* ProjectionSettings);
		void ComputeNormal(FCluster* InCluster);
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
	};

	class PCGEXTENDEDTOOLKIT_API TClusterFilter : public PCGExDataFilter::TFilter
	{
	public:
		explicit TClusterFilter(const UPCGExClusterFilterFactoryBase* InFactory)
			: TFilter(InFactory)
		{
			bValid = false;
		}

		const FCluster* CapturedCluster = nullptr;

		FORCEINLINE virtual PCGExDataFilter::EType GetFilterType() const override;

		virtual void CaptureCluster(const FPCGContext* InContext, const FCluster* InCluster);
		virtual void Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO) override;
		virtual void CaptureEdges(const FPCGContext* InContext, const PCGExData::FPointIO* EdgeIO);
		virtual bool PrepareForTesting(const PCGExData::FPointIO* PointIO) override;
	};

	class PCGEXTENDEDTOOLKIT_API FNodeStateHandler : public PCGExDataState::TDataState
	{
	public:
		explicit FNodeStateHandler(const UPCGExNodeStateFactory* InFactory);

		const UPCGExNodeStateFactory* NodeStateDefinition = nullptr;

		TArray<TFilter*> FilterHandlers;
		TArray<TFilter*> HeavyFilterHandlers;
		TArray<TClusterFilter*> ClusterFilterHandlers;
		TArray<TClusterFilter*> HeavyClusterFilterHandlers;

		virtual void CaptureCluster(const FPCGContext* InContext, FCluster* InCluster);
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override;

		virtual bool PrepareForTesting(const PCGExData::FPointIO* PointIO) override;
		virtual void PrepareSingle(const int32 PointIndex) override;
		virtual void PreparationComplete() override;

		virtual bool RequiresPerPointPreparation() const;

		virtual ~FNodeStateHandler() override
		{
			PCGEX_DELETE_TARRAY(FilterHandlers)
			HeavyFilterHandlers.Empty();

			PCGEX_DELETE_TARRAY(ClusterFilterHandlers)
			HeavyClusterFilterHandlers.Empty();
		}

	protected:
		PCGExData::FPointIO* LastPoints = nullptr;
		FCluster* Cluster = nullptr;
	};
}

namespace PCGExClusterTask
{
	class PCGEXTENDEDTOOLKIT_API FBuildCluster : public FPCGExNonAbandonableTask
	{
	public:
		FBuildCluster(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		              PCGExCluster::FCluster* InCluster,
		              PCGExData::FPointIO* InEdgeIO,
		              TMap<int64, int32>* InNodeIndicesMap,
		              TArray<int32>* InPerNodeEdgeNums) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			Cluster(InCluster),
			EdgeIO(InEdgeIO),
			NodeIndicesMap(InNodeIndicesMap),
			PerNodeEdgeNums(InPerNodeEdgeNums)
		{
		}

		PCGExCluster::FCluster* Cluster = nullptr;
		PCGExData::FPointIO* EdgeIO = nullptr;
		TMap<int64, int32>* NodeIndicesMap = nullptr;
		TArray<int32>* PerNodeEdgeNums = nullptr;

		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FFindNodeChains : public FPCGExNonAbandonableTask
	{
	public:
		FFindNodeChains(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		                PCGExCluster::FCluster* InCluster,
		                TSet<int32>* InFixtures,
		                TArray<PCGExCluster::FNodeChain*>* InChains) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			Cluster(InCluster),
			Fixtures(InFixtures),
			Chains(InChains)
		{
		}

		PCGExCluster::FCluster* Cluster = nullptr;
		TSet<int32>* Fixtures = nullptr;
		TArray<PCGExCluster::FNodeChain*>* Chains = nullptr;

		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FProjectCluster : public FPCGExNonAbandonableTask
	{
	public:
		FProjectCluster(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		                PCGExCluster::FCluster* InCluster, PCGExCluster::FClusterProjection* InProjection) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			Cluster(InCluster),
			Projection(InProjection)
		{
		}

		PCGExCluster::FCluster* Cluster = nullptr;
		PCGExCluster::FClusterProjection* Projection = nullptr;

		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FCopyClustersToPoint : public FPCGExNonAbandonableTask
	{
	public:
		FCopyClustersToPoint(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		                     PCGExData::FPointIO* InVtx,
		                     const TArray<PCGExData::FPointIO*>& InEdges,
		                     PCGExData::FPointIOCollection* InVtxCollection,
		                     PCGExData::FPointIOCollection* InEdgeCollection,
		                     FPCGExTransformSettings* InTransformSettings) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
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
