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

UENUM(BlueprintType)
enum class EPCGExClusterClosestSearchMode : uint8
{
	Node UMETA(DisplayName = "Closest node", ToolTip="Proximity to node position"),
	Edge UMETA(DisplayName = "Closest edge", ToolTip="Proximity to edge, then endpoint"),
};

UENUM(BlueprintType)
enum class EPCGExClusterSearchOrientationMode : uint8
{
	CCW UMETA(DisplayName = "Counter Clockwise"),
	CW UMETA(DisplayName = "Clockwise"),
};


namespace PCGExCluster
{
	const FName OutputNodeStateLabel = TEXT("NodeState");
	const FName SourceNodeStateLabel = TEXT("NodeStates");

	constexpr PCGExMT::AsyncState State_ProjectingCluster = __COUNTER__;

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

		void AddConnection(const int32 EdgeIndex, const int32 NodeIndex);
		FVector GetCentroid(FCluster* InCluster) const;
		int32 GetEdgeIndex(int32 AdjacentNodeIndex) const;
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
		FCluster();

		~FCluster();

		bool BuildFrom(
			const PCGExData::FPointIO& EdgeIO,
			const TArray<FPCGPoint>& InNodePoints,
			const TMap<int64, int32>& InNodeIndicesMap,
			const TArray<int32>& PerNodeEdgeNums);

		void BuildPartialFrom(const TArray<FVector>& Positions, const TSet<uint64>& InEdges);

		int32 FindClosestNode(const FVector& Position, EPCGExClusterClosestSearchMode Mode, const int32 MinNeighbors = 0) const;
		int32 FindClosestNode(const FVector& Position, const int32 MinNeighbors = 0) const;
		int32 FindClosestNodeFromEdge(const FVector& Position, const int32 MinNeighbors = 0) const;

		int32 FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, int32 MinNeighborCount = 1) const;
		int32 FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, const TSet<int32>& Exclusion, int32 MinNeighborCount = 1) const;

		const FNode& GetNodeFromPointIndex(const int32 Index) const;
		const PCGExGraph::FIndexedEdge& GetEdgeFromNodeIndices(const int32 A, const int32 B) const;
		void ComputeEdgeLengths(bool bNormalize = false);

		void GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth) const;

		FVector GetEdgeDirection(const int32 FromIndex, const int32 ToIndex) const;
		FVector GetCentroid(const int32 NodeIndex) const;

		int32 FindClosestNeighborInDirection(const int32 NodeIndex, const FVector& Direction, int32 MinNeighborCount = 1) const;

	protected:
		FNode& GetOrCreateNode(const int32 PointIndex, const TArray<FPCGPoint>& InPoints);
	};

	struct PCGEXTENDEDTOOLKIT_API FNodeProjection
	{
		FNode* Node = nullptr;
		FVector Normal = FVector::UpVector;
		TArray<int32> SortedAdjacency;

		FNodeProjection(FNode* InNode);

		void Project(FCluster* InCluster, const FPCGExGeo2DProjectionSettings* ProjectionSettings);
		void ComputeNormal(FCluster* InCluster);
		int32 GetAdjacencyIndex(int32 NodeIndex) const;

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
		int32 FindNextAdjacentNode(EPCGExClusterSearchOrientationMode Orient, int32 NodeIndex, int32 From, const TSet<int32>& Exclusion, const int32 MinNeighbors);
		int32 FindNextAdjacentNodeCCW(int32 NodeIndex, int32 From, const TSet<int32>& Exclusion, const int32 MinNeighbors);
		int32 FindNextAdjacentNodeCW(int32 NodeIndex, int32 From, const TSet<int32>& Exclusion, const int32 MinNeighbors);
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


	class PCGEXTENDEDTOOLKIT_API FNodeTestHandler
	{
	public:
		explicit FNodeTestHandler(UPCGExAdjacencyTestDefinition* InDefinition)
			: Definition(InDefinition)
		{
		}

		virtual ~FNodeTestHandler()
		{
			Definition = nullptr;
			PCGEX_DELETE(OperandAGetter)
			PCGEX_DELETE(OperandBGetter)
		}

		UPCGExAdjacencyTestDefinition* Definition = nullptr;
		PCGEx::FLocalSingleFieldGetter* OperandAGetter = nullptr;
		PCGEx::FLocalSingleFieldGetter* OperandBGetter = nullptr;

		virtual bool Test(const int32 PointIndex) const;
	};

	class PCGEXTENDEDTOOLKIT_API FNodeStateHandler : public PCGExDataState::TStateHandler
	{
	public:
		explicit FNodeStateHandler(UPCGExNodeStateDefinition* InDefinition);

		UPCGExNodeStateDefinition* Definition = nullptr;
		TArray<FNodeTestHandler*> TestHandlers;

		void CaptureCluster(FCluster* InCluster);
		virtual bool Test(const int32 PointIndex) const override;

		virtual ~FNodeStateHandler() override
		{
			PCGEX_DELETE_TARRAY(TestHandlers)
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
}
