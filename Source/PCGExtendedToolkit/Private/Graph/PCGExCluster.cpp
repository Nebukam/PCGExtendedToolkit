// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCluster.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Geometry/PCGExGeo.h"

#pragma region UPCGExNodeStateDefinition

PCGExDataFilter::TFilterHandler* UPCGExNodeStateDefinition::CreateHandler() const
{
	return new PCGExCluster::FNodeStateHandler(this);
}

void UPCGExNodeStateDefinition::BeginDestroy()
{
	Super::BeginDestroy();
}

#pragma endregion

namespace PCGExCluster
{
#pragma region FNode

	FNode::~FNode()
	{
		AdjacentNodes.Empty();
		Edges.Empty();
	}

	void FNode::AddConnection(const int32 InEdgeIndex, const int32 InNodeIndex)
	{
		Edges.AddUnique(InEdgeIndex);
		AdjacentNodes.AddUnique(InNodeIndex);
	}

	FVector FNode::GetCentroid(FCluster* InCluster) const
	{
		if (AdjacentNodes.IsEmpty()) { return Position; }

		FVector Centroid = FVector::ZeroVector;
		const int32 NumPoints = AdjacentNodes.Num();

		for (int i = 0; i < NumPoints; i++) { Centroid += InCluster->Nodes[AdjacentNodes[i]].Position; }

		if (AdjacentNodes.Num() < 2)
		{
			Centroid += Position;
			return Centroid / 2;
		}

		Centroid /= static_cast<double>(NumPoints);

		return Centroid;
	}

	int32 FNode::GetEdgeIndex(const int32 AdjacentNodeIndex) const
	{
		for (int i = 0; i < AdjacentNodes.Num(); i++) { if (AdjacentNodes[i] == AdjacentNodeIndex) { return Edges[i]; } }
		return -1;
	}

#pragma endregion

#pragma region FCluster

	FCluster::FCluster()
	{
		PointIndexMap.Empty();
		Nodes.Empty();
		Edges.Empty();
		Bounds = FBox(ForceInit);
	}

	FCluster::~FCluster()
	{
		Nodes.Empty();
		PointIndexMap.Empty();
		Edges.Empty();
		EdgeLengths.Empty();
	}

	FNode& FCluster::GetOrCreateNode(const int32 PointIndex, const TArray<FPCGPoint>& InPoints)
	{
		if (const int32* NodeIndex = PointIndexMap.Find(PointIndex)) { return Nodes[*NodeIndex]; }

		FNode& Node = Nodes.Emplace_GetRef();
		const int32 NodeIndex = Nodes.Num() - 1;

		PointIndexMap.Add(PointIndex, NodeIndex);

		Node.PointIndex = PointIndex;
		Node.NodeIndex = NodeIndex;
		Node.Position = InPoints[PointIndex].Transform.GetLocation();

		Bounds += Node.Position;

		return Node;
	}

	bool FCluster::BuildFrom(
		const PCGExData::FPointIO& EdgeIO,
		const TArray<FPCGPoint>& InNodePoints,
		const TMap<int64, int32>& InNodeIndicesMap,
		const TArray<int32>& PerNodeEdgeNums)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCluster::BuildCluster);

		Nodes.Empty();
		Edges.Empty();
		PointIndexMap.Empty();
		EdgeIndexMap.Empty();

		TArray<PCGExGraph::FIndexedEdge> EdgeList;
		if (!BuildIndexedEdges(EdgeIO, InNodeIndicesMap, EdgeList, true))
		{
			EdgeList.Empty();
			return false;
		}

		bool bInvalidCluster = false;

		const int32 NumNodes = InNodePoints.Num();
		Nodes.Reserve(NumNodes);
		PointIndexMap.Reserve(NumNodes);

		const int32 NumEdges = EdgeList.Num();
		Edges.SetNumUninitialized(NumEdges);
		EdgeIndexMap.Reserve(NumEdges);

		//We need to sort edges in order to have deterministic processing of the clusters
		EdgeList.Sort(
			[](const PCGExGraph::FIndexedEdge& A, const PCGExGraph::FIndexedEdge& B)
			{
				return A.Start == B.Start ? A.End < B.End : A.Start < B.Start;
			});

		for (int i = 0; i < NumEdges; i++)
		{
			PCGExGraph::FIndexedEdge& SortedEdge = (Edges[i] = EdgeList[i]);
			SortedEdge.EdgeIndex = i;

			FNode& Start = GetOrCreateNode(SortedEdge.Start, InNodePoints);
			FNode& End = GetOrCreateNode(SortedEdge.End, InNodePoints);
			EdgeIndexMap.Add(PCGEx::H64U(Start.NodeIndex, End.NodeIndex), i);

			Start.AddConnection(i, End.NodeIndex);
			End.AddConnection(i, Start.NodeIndex);
		}

		EdgeList.Empty();

		for (FNode& Node : Nodes)
		{
			if (PerNodeEdgeNums[Node.PointIndex] > Node.AdjacentNodes.Num()) // We care about removed connections, not new ones 
			{
				bInvalidCluster = true;
				break;
			}
		}

		bValid = !bInvalidCluster;
		return bValid;
	}

	void FCluster::BuildPartialFrom(const TArray<FVector>& Positions, const TSet<uint64>& InEdges)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCluster::BuildCluster);

		const int32 NumNodes = Positions.Num();
		Nodes.SetNum(NumNodes);

		const int32 NumEdges = InEdges.Num();
		Edges.SetNumUninitialized(NumEdges);

		for (int i = 0; i < NumNodes; i++)
		{
			FNode& Node = Nodes[i];

			Node.PointIndex = i;
			Node.NodeIndex = i;
			Node.Position = Positions[i];
		}

		int32 EdgeIndex = 0;
		for (const uint64 Edge : InEdges)
		{
			uint32 A;
			uint32 B;
			PCGEx::H64(Edge, A, B);
			FNode& Start = Nodes[A];
			FNode& End = Nodes[B];

			Start.AddConnection(-1, End.NodeIndex);
			End.AddConnection(-1, Start.NodeIndex);
			EdgeIndex++;
		}
	}

	int32 FCluster::FindClosestNode(const FVector& Position, EPCGExClusterClosestSearchMode Mode, const int32 MinNeighbors) const
	{
		switch (Mode)
		{
		default: ;
		case EPCGExClusterClosestSearchMode::Node:
			return FindClosestNode(Position, MinNeighbors);
		case EPCGExClusterClosestSearchMode::Edge:
			return FindClosestNodeFromEdge(Position, MinNeighbors);
		}
	}

	int32 FCluster::FindClosestNode(const FVector& Position, const int32 MinNeighbors) const
	{
		double MaxDistance = TNumericLimits<double>::Max();
		int32 ClosestIndex = -1;
		for (const FNode& Node : Nodes)
		{
			if (Node.AdjacentNodes.Num() < MinNeighbors) { continue; }
			const double Dist = FVector::DistSquared(Position, Node.Position);
			if (Dist < MaxDistance)
			{
				MaxDistance = Dist;
				ClosestIndex = Node.NodeIndex;
			}
		}

		return ClosestIndex;
	}

	int32 FCluster::FindClosestNodeFromEdge(const FVector& Position, const int32 MinNeighbors) const
	{
		double MaxDistance = TNumericLimits<double>::Max();
		int32 ClosestIndex = -1;

		for (const PCGExGraph::FIndexedEdge& Edge : Edges)
		{
			const FNode& Start = Nodes[*PointIndexMap.Find(Edge.Start)];
			const FNode& End = Nodes[*PointIndexMap.Find(Edge.End)];
			const double Dist = FMath::PointDistToSegmentSquared(Position, Start.Position, End.Position);
			if (Dist < MaxDistance)
			{
				MaxDistance = Dist;
				ClosestIndex = Edge.EdgeIndex;
			}
		}

		if (ClosestIndex == -1) { return -1; }

		const PCGExGraph::FIndexedEdge& Edge = Edges[ClosestIndex];
		const FNode& Start = Nodes[*PointIndexMap.Find(Edge.Start)];
		const FNode& End = Nodes[*PointIndexMap.Find(Edge.End)];

		ClosestIndex = FVector::DistSquared(Position, Start.Position) < FVector::DistSquared(Position, End.Position) ? Start.NodeIndex : End.NodeIndex;

		return ClosestIndex;
	}

	int32 FCluster::FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, int32 MinNeighborCount) const
	{
		const FNode& Node = Nodes[NodeIndex];
		int32 Result = -1;
		double LastDist = TNumericLimits<double>::Max();

		for (const int32 OtherIndex : Node.AdjacentNodes)
		{
			if (Nodes[OtherIndex].AdjacentNodes.Num() < MinNeighborCount) { continue; }
			if (const double Dist = FMath::PointDistToSegmentSquared(Position, Node.Position, Nodes[OtherIndex].Position);
				Dist < LastDist)
			{
				LastDist = Dist;
				Result = OtherIndex;
			}
		}

		return Result;
	}

	int32 FCluster::FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, const TSet<int32>& Exclusion, int32 MinNeighborCount) const
	{
		const FNode& Node = Nodes[NodeIndex];
		int32 Result = -1;
		double LastDist = TNumericLimits<double>::Max();

		for (const int32 OtherIndex : Node.AdjacentNodes)
		{
			if (Nodes[OtherIndex].AdjacentNodes.Num() < MinNeighborCount) { continue; }
			if (Exclusion.Contains(OtherIndex)) { continue; }
			if (const double Dist = FMath::PointDistToSegmentSquared(Position, Node.Position, Nodes[OtherIndex].Position);
				Dist < LastDist)
			{
				LastDist = Dist;
				Result = OtherIndex;
			}
		}

		return Result;
	}

	const FNode& FCluster::GetNodeFromPointIndex(const int32 Index) const { return Nodes[*PointIndexMap.Find(Index)]; }

	const PCGExGraph::FIndexedEdge& FCluster::GetEdgeFromNodeIndices(const int32 A, const int32 B) const { return Edges[*EdgeIndexMap.Find(PCGEx::H64U(A, B))]; }

	void FCluster::ComputeEdgeLengths(const bool bNormalize)
	{
		if (!bEdgeLengthsDirty) { return; }

		const int32 NumEdges = Edges.Num();
		double Min = TNumericLimits<double>::Max();
		double Max = TNumericLimits<double>::Min();
		EdgeLengths.SetNumUninitialized(NumEdges);

		for (int i = 0; i < NumEdges; i++)
		{
			const PCGExGraph::FIndexedEdge& Edge = Edges[i];
			const FNode& A = GetNodeFromPointIndex(Edge.Start);
			const FNode& B = GetNodeFromPointIndex(Edge.End);
			const double Dist = FVector::DistSquared(A.Position, B.Position);
			EdgeLengths[i] = Dist;
			Min = FMath::Min(Dist, Min);
			Max = FMath::Max(Dist, Max);
		}

		if (bNormalize) { for (int i = 0; i < NumEdges; i++) { EdgeLengths[i] = PCGExMath::Remap(EdgeLengths[i], Min, Max, 0, 1); } }

		bEdgeLengthsDirty = false;
	}

	void FCluster::GetNodePointIndices(TArray<int32>& OutIndices)
	{
		int32 Offset = OutIndices.Num();
		OutIndices.SetNum(Offset + Nodes.Num());
		for (const FNode& Node : Nodes) { OutIndices[Offset++] = Node.PointIndex; }
	}

	void FCluster::GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth) const
	{
		const int32 NextDepth = SearchDepth - 1;
		const FNode& RootNode = Nodes[FromIndex];

		for (const int32 OtherNode : RootNode.AdjacentNodes)
		{
			if (OutIndices.Contains(OtherNode)) { continue; }

			OutIndices.Add(OtherNode);
			if (NextDepth > 0) { GetConnectedNodes(OtherNode, OutIndices, NextDepth); }
		}
	}

	FVector FCluster::GetEdgeDirection(const int32 FromIndex, const int32 ToIndex) const
	{
		return (Nodes[FromIndex].Position - Nodes[ToIndex].Position).GetSafeNormal();
	}

	FVector FCluster::GetCentroid(const int32 NodeIndex) const
	{
		const FNode& Node = Nodes[NodeIndex];
		FVector Centroid = FVector::ZeroVector;
		for (const int32 OtherIndex : Node.AdjacentNodes) { Centroid += Nodes[OtherIndex].Position; }
		return Centroid / static_cast<double>(Node.AdjacentNodes.Num());
	}

	int32 FCluster::FindClosestNeighborInDirection(const int32 NodeIndex, const FVector& Direction, const int32 MinNeighbors) const
	{
		const FNode& Node = Nodes[NodeIndex];
		int32 Result = -1;
		double LastDot = -1;

		for (const int32 OtherIndex : Node.AdjacentNodes)
		{
			if (Nodes[OtherIndex].AdjacentNodes.Num() < MinNeighbors) { continue; }
			if (const double Dot = FVector::DotProduct(Direction, (Node.Position - Nodes[OtherIndex].Position).GetSafeNormal());
				Dot > LastDot)
			{
				LastDot = Dot;
				Result = OtherIndex;
			}
		}

		return Result;
	}

#pragma endregion

#pragma region FNodeProjection

	FNodeProjection::FNodeProjection(FNode* InNode)
		: Node(InNode)
	{
	}

	void FNodeProjection::Project(FCluster* InCluster, const FPCGExGeo2DProjectionSettings* ProjectionSettings)
	{
		Normal = FVector::UpVector;

		const int32 NumNodes = Node->AdjacentNodes.Num();
		SortedAdjacency.SetNum(NumNodes);

		TArray<int32> Sort;
		TArray<double> Angles;

		Sort.SetNum(NumNodes);
		Angles.SetNum(NumNodes);

		for (int i = 0; i < NumNodes; i++)
		{
			Sort[i] = i;
			FVector Direction = ProjectionSettings->Project((Node->Position - InCluster->Nodes[Node->AdjacentNodes[i]].Position), Node->PointIndex);
			Direction.Z = 0;
			Angles[i] = PCGExMath::GetAngle(FVector::ForwardVector, Direction.GetSafeNormal());
		}

		Sort.Sort([&](const int32& A, const int32& B) { return Angles[A] > Angles[B]; });
		for (int i = 0; i < NumNodes; i++) { SortedAdjacency[i] = Node->AdjacentNodes[Sort[i]]; }

		Sort.Empty();
		Angles.Empty();
	}

	void FNodeProjection::ComputeNormal(FCluster* InCluster)
	{
		Normal = FVector::ZeroVector;

		if (SortedAdjacency.IsEmpty())
		{
			Normal = FVector::UpVector;
			return;
		}

		Normal = PCGExMath::GetNormal(InCluster->Nodes[SortedAdjacency.Last()].Position, Node->Position, InCluster->Nodes[SortedAdjacency[0]].Position);

		if (SortedAdjacency.Num() < 2) { return; }

		for (int i = 0; i < SortedAdjacency.Num() - 1; i++)
		{
			Normal += PCGExMath::GetNormal(InCluster->Nodes[SortedAdjacency[i]].Position, Node->Position, InCluster->Nodes[SortedAdjacency[i + 1]].Position);
		}

		Normal /= SortedAdjacency.Num();
	}

	int32 FNodeProjection::GetAdjacencyIndex(const int32 NodeIndex) const
	{
		for (int i = 0; i < SortedAdjacency.Num(); i++) { if (SortedAdjacency[i] == NodeIndex) { return i; } }
		return -1;
	}

	FNodeProjection::~FNodeProjection()
	{
		SortedAdjacency.Empty();
	}

#pragma endregion

#pragma region FClusterProjection

	FClusterProjection::FClusterProjection(FCluster* InCluster, FPCGExGeo2DProjectionSettings* InProjectionSettings)
		: Cluster(InCluster), ProjectionSettings(InProjectionSettings)
	{
		Nodes.Reserve(Cluster->Nodes.Num());
		for (FNode& Node : Cluster->Nodes) { Nodes.Emplace(&Node); }
	}

	FClusterProjection::~FClusterProjection()
	{
		Nodes.Empty();
	}

	void FClusterProjection::Build()
	{
		for (FNodeProjection& PNode
		     :
		     Nodes
		)
		{
			PNode.Project(Cluster, ProjectionSettings);
		}
	}

	int32 FClusterProjection::FindNextAdjacentNode(EPCGExClusterSearchOrientationMode Orient, int32 NodeIndex, int32 From, const TSet<int32>& Exclusion, const int32 MinNeighbors)
	{
		if (Orient == EPCGExClusterSearchOrientationMode::CW) { return FindNextAdjacentNodeCW(NodeIndex, From, Exclusion, MinNeighbors); }
		return FindNextAdjacentNodeCCW(NodeIndex, From, Exclusion, MinNeighbors);
	}

	int32 FClusterProjection::FindNextAdjacentNodeCCW(const int32 NodeIndex, const int32 From, const TSet<int32>& Exclusion, const int32 MinNeighbors)
	{
		const FNodeProjection& Project = Nodes[NodeIndex];
		const int32 StartIndex = Project.GetAdjacencyIndex(From);
		if (StartIndex == -1) { return -1; }

		const int32 NumNodes = Project.SortedAdjacency.Num();
		for (int i = 0; i < NumNodes; i++)
		{
			const int32 NextIndex = Project.SortedAdjacency[PCGExMath::Tile(StartIndex + i + 1, 0, NumNodes - 1)];
			if ((NextIndex == From && NumNodes > 1) ||
				Exclusion.Contains(NextIndex) ||
				Cluster->Nodes[NextIndex].AdjacentNodes.Num() < MinNeighbors) { continue; }

			return NextIndex;
		}

		return -1;
	}

	int32 FClusterProjection::FindNextAdjacentNodeCW(const int32 NodeIndex, const int32 From, const TSet<int32>& Exclusion, const int32 MinNeighbors)
	{
		const FNodeProjection& Project = Nodes[NodeIndex];
		const int32 StartIndex = Project.GetAdjacencyIndex(From);
		if (StartIndex == -1) { return -1; }

		const int32 NumNodes = Project.SortedAdjacency.Num();
		for (int i = 0; i < NumNodes; i++)
		{
			const int32 NextIndex = Project.SortedAdjacency[PCGExMath::Tile(StartIndex - i - 1, 0, NumNodes - 1)];
			if ((NextIndex == From && NumNodes > 1) ||
				Exclusion.Contains(NextIndex) ||
				Cluster->Nodes[NextIndex].AdjacentNodes.Num() < MinNeighbors) { continue; }

			return NextIndex;
		}

		return -1;
	}

#pragma endregion

#pragma region FNodeStateHandler

	void TClusterFilterHandler::CaptureCluster(const FCluster* InCluster)
	{
		bValid = true;
		TestedCluster = InCluster;
		Capture(InCluster->PointsIO);
		CaptureEdges(InCluster->EdgesIO);
	}

	void TClusterFilterHandler::Capture(const PCGExData::FPointIO* PointIO)
	{
		//TFilterHandler::Capture(PointIO); //Do not call Super:: as it sets bValid to true.
	}

	void TClusterFilterHandler::CaptureEdges(const PCGExData::FPointIO* EdgeIO)
	{
	}

	FNodeStateHandler::FNodeStateHandler(const UPCGExNodeStateDefinition* InDefinition)
		: TStateHandler(InDefinition), NodeStateDefinition(InDefinition)
	{
		const int32 NumConditions = InDefinition->Tests.Num();

		FilterHandlers.Empty();
		ClusterFilterHandlers.Empty();

		for (int i = 0; i < NumConditions; i++)
		{
			TFilterHandler* Handler = InDefinition->Tests[i]->CreateHandler();
			if (TClusterFilterHandler* ClusterHandler = dynamic_cast<TClusterFilterHandler*>(Handler)) { ClusterFilterHandlers.Add(ClusterHandler); }
			else { FilterHandlers.Add(Handler); }
		}
	}

	void FNodeStateHandler::CaptureCluster(FCluster* InCluster)
	{
		Cluster = InCluster;
		for (TFilterHandler* Test : FilterHandlers) { Test->Capture(Cluster->PointsIO); }
		for (TClusterFilterHandler* Test : ClusterFilterHandlers) { Test->CaptureCluster(Cluster); }
	}

	bool FNodeStateHandler::Test(const int32 PointIndex) const
	{
		if (!FilterHandlers.IsEmpty())
		{
			const int32 PtIndex = Cluster->GetNodeFromPointIndex(PointIndex).PointIndex; // We get a node index from the FindNode
			for (const TFilterHandler* Test : FilterHandlers) { if (!Test->Test(PtIndex)) { return false; } }
		}

		for (const TClusterFilterHandler* Test : ClusterFilterHandlers) { if (!Test->Test(PointIndex)) { return false; } }

		return true;
	}

#pragma endregion
}

namespace PCGExClusterTask
{
	bool FBuildCluster::ExecuteTask()
	{
		Cluster->BuildFrom(
			*EdgeIO,
			PointIO->GetIn()->GetPoints(),
			*NodeIndicesMap,
			*PerNodeEdgeNums);

		return true;
	}

	bool FFindNodeChains::ExecuteTask()
	{
		return true;
	}

	bool FProjectCluster::ExecuteTask()
	{
		return true;
	}
}
