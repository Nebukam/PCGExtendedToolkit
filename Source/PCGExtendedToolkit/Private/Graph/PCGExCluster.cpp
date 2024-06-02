// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCluster.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Geometry/PCGExGeo.h"

#pragma region UPCGExNodeStateDefinition

PCGExFactories::EType UPCGExClusterFilterFactoryBase::GetFactoryType() const
{
	return  PCGExFactories::EType::ClusterFilter;
}

PCGExFactories::EType UPCGExNodeStateFactory::GetFactoryType() const
{
	return PCGExFactories::EType::NodeState;
}

PCGExDataFilter::TFilter* UPCGExNodeStateFactory::CreateFilter() const
{
	return new PCGExCluster::FNodeStateHandler(this);
}

void UPCGExNodeStateFactory::BeginDestroy()
{
	FilterFactories.Empty();
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

		PCGEX_DELETE(NodeOctree)
		PCGEX_DELETE(EdgeOctree)
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

		/*
		//We use to need to sort cluster to get deterministic results but it seems not to be the case anymore
		EdgeList.Sort(
			[](const PCGExGraph::FIndexedEdge& A, const PCGExGraph::FIndexedEdge& B)
			{
				return A.Start == B.Start ? A.End < B.End : A.Start < B.Start;
			});
		*/

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

	void FCluster::RebuildNodeOctree()
	{
		PCGEX_DELETE(NodeOctree)
		NodeOctree = new ClusterItemOctree(Bounds.GetCenter(), Bounds.GetExtent().Length());
		for (const FNode& Node : Nodes)
		{
			NodeOctree->AddElement(FClusterItemRef(Node.NodeIndex, FBoxSphereBounds(FSphere(Node.Position, 0))));
		}
	}

	void FCluster::RebuildEdgeOctree()
	{
		PCGEX_DELETE(EdgeOctree)
		EdgeOctree = new ClusterItemOctree(Bounds.GetCenter(), Bounds.GetExtent().Length());
		for (const PCGExGraph::FIndexedEdge& Edge : Edges)
		{
			const FNode& Start = Nodes[*PointIndexMap.Find(Edge.Start)];
			const FNode& End = Nodes[*PointIndexMap.Find(Edge.End)];
			EdgeOctree->AddElement(
					FClusterItemRef(
							Edge.EdgeIndex,
							FBoxSphereBounds(
									FSphere(
										FMath::Lerp(Start.Position, End.Position, 0.5),
										FVector::Dist(Start.Position, End.Position) * 0.5)
								)
						)
				);
		}
	}

	void FCluster::RebuildOctree(const EPCGExClusterClosestSearchMode Mode)
	{
		switch (Mode)
		{
		case EPCGExClusterClosestSearchMode::Node:
			RebuildNodeOctree();
			break;
		case EPCGExClusterClosestSearchMode::Edge:
			RebuildEdgeOctree();
			break;
		default: ;
		}
	}

	int32 FCluster::FindClosestNode(const FVector& Position, const EPCGExClusterClosestSearchMode Mode, const int32 MinNeighbors) const
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

		if (NodeOctree)
		{
			auto ProcessCandidate = [&](const FClusterItemRef& Item)
			{
				const FNode& Node = Nodes[Item.ItemIndex];
				if (Node.AdjacentNodes.Num() < MinNeighbors) { return; }
				const double Dist = FVector::DistSquared(Position, Node.Position);
				if (Dist < MaxDistance)
				{
					MaxDistance = Dist;
					ClosestIndex = Node.NodeIndex;
				}
			};

			NodeOctree->FindNearbyElements(Position, ProcessCandidate);
		}
		else
		{
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
		}

		return ClosestIndex;
	}

	int32 FCluster::FindClosestNodeFromEdge(const FVector& Position, const int32 MinNeighbors) const
	{
		double MaxDistance = TNumericLimits<double>::Max();
		int32 ClosestIndex = -1;

		if (EdgeOctree)
		{
			auto ProcessCandidate = [&](const FClusterItemRef& Item)
			{
				const PCGExGraph::FIndexedEdge& Edge = Edges[Item.ItemIndex];
				const FNode& Start = Nodes[*PointIndexMap.Find(Edge.Start)];
				const FNode& End = Nodes[*PointIndexMap.Find(Edge.End)];
				const double Dist = FMath::PointDistToSegmentSquared(Position, Start.Position, End.Position);
				if (Dist < MaxDistance)
				{
					MaxDistance = Dist;
					ClosestIndex = Edge.EdgeIndex;
				}
			};

			EdgeOctree->FindNearbyElements(Position, ProcessCandidate);
		}
		else
		{
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
		}

		if (ClosestIndex == -1) { return -1; }

		const PCGExGraph::FIndexedEdge& Edge = Edges[ClosestIndex];
		const FNode& Start = Nodes[*PointIndexMap.Find(Edge.Start)];
		const FNode& End = Nodes[*PointIndexMap.Find(Edge.End)];

		ClosestIndex = FVector::DistSquared(Position, Start.Position) < FVector::DistSquared(Position, End.Position) ? Start.NodeIndex : End.NodeIndex;

		return ClosestIndex;
	}

	int32 FCluster::FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, const int32 MinNeighborCount) const
	{
		const FNode& Node = Nodes[NodeIndex];
		int32 Result = -1;
		double LastDist = TNumericLimits<double>::Max();

		if (NodeOctree)
		{
			auto ProcessCandidate = [&](const FClusterItemRef& Item)
			{
				if (Nodes[Item.ItemIndex].AdjacentNodes.Num() < MinNeighborCount) { return; }
				if (const double Dist = FMath::PointDistToSegmentSquared(Position, Node.Position, Nodes[Item.ItemIndex].Position);
					Dist < LastDist)
				{
					LastDist = Dist;
					Result = Item.ItemIndex;
				}
			};

			NodeOctree->FindNearbyElements(Position, ProcessCandidate);
		}
		else
		{
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
		}

		return Result;
	}

	int32 FCluster::FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, const TSet<int32>& Exclusion, const int32 MinNeighborCount) const
	{
		const FNode& Node = Nodes[NodeIndex];
		int32 Result = -1;
		double LastDist = TNumericLimits<double>::Max();

		if (NodeOctree)
		{
			auto ProcessCandidate = [&](const FClusterItemRef& Item)
			{
				if (Nodes[Item.ItemIndex].AdjacentNodes.Num() < MinNeighborCount) { return; }
				if (Exclusion.Contains(Item.ItemIndex)) { return; }
				if (const double Dist = FMath::PointDistToSegmentSquared(Position, Node.Position, Nodes[Item.ItemIndex].Position);
					Dist < LastDist)
				{
					LastDist = Dist;
					Result = Item.ItemIndex;
				}
			};

			NodeOctree->FindNearbyElements(Position, ProcessCandidate);
		}
		else
		{
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

		//Normalized to 0 instead of min
		if (bNormalize) { for (int i = 0; i < NumEdges; i++) { EdgeLengths[i] = PCGExMath::Remap(EdgeLengths[i], 0, Max, 0, 1); } }

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

	void FCluster::GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth, const TSet<int32>& Skip) const
	{
		const int32 NextDepth = SearchDepth - 1;
		const FNode& RootNode = Nodes[FromIndex];

		for (const int32 OtherNode : RootNode.AdjacentNodes)
		{
			if (Skip.Contains(OtherNode) || OutIndices.Contains(OtherNode)) { continue; }

			OutIndices.Add(OtherNode);
			if (NextDepth > 0) { GetConnectedNodes(OtherNode, OutIndices, NextDepth, Skip); }
		}
	}

	void FCluster::GetConnectedEdges(const int32 FromNodeIndex, TArray<int32>& OutNodeIndices, TArray<int32>& OutEdgeIndices, const int32 SearchDepth) const
	{
		const int32 NextDepth = SearchDepth - 1;
		const FNode& RootNode = Nodes[FromNodeIndex];

		for (const int32 OtherNode : RootNode.AdjacentNodes)
		{
			if (OutNodeIndices.Contains(OtherNode)) { continue; }

			const int32* EdgeIndex = EdgeIndexMap.Find(PCGEx::H64U(FromNodeIndex, OtherNode));

			if (!EdgeIndex || OutEdgeIndices.Contains(*EdgeIndex)) { continue; }

			OutNodeIndices.Add(OtherNode);
			OutEdgeIndices.Add(*EdgeIndex);

			if (NextDepth > 0) { GetConnectedEdges(OtherNode, OutNodeIndices, OutEdgeIndices, NextDepth); }
		}
	}

	void FCluster::GetConnectedEdges(const int32 FromNodeIndex, TArray<int32>& OutNodeIndices, TArray<int32>& OutEdgeIndices, const int32 SearchDepth, const TSet<int32>& SkipNodes, const TSet<int32>& SkipEdges) const
	{
		const int32 NextDepth = SearchDepth - 1;
		const FNode& RootNode = Nodes[FromNodeIndex];

		for (const int32 OtherNode : RootNode.AdjacentNodes)
		{
			if (SkipNodes.Contains(OtherNode) || OutNodeIndices.Contains(OtherNode)) { continue; }

			const int32* EdgeIndex = EdgeIndexMap.Find(PCGEx::H64U(FromNodeIndex, OtherNode));

			if (!EdgeIndex || SkipEdges.Contains(*EdgeIndex) || OutEdgeIndices.Contains(*EdgeIndex)) { continue; }

			OutNodeIndices.Add(OtherNode);
			OutEdgeIndices.Add(*EdgeIndex);

			if (NextDepth > 0) { GetConnectedEdges(OtherNode, OutNodeIndices, OutEdgeIndices, NextDepth, SkipNodes, SkipEdges); }
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

	int32 FCluster::FindClosestNeighborInDirection(const int32 NodeIndex, const FVector& Direction, const int32 MinNeighborCount) const
	{
		const FNode& Node = Nodes[NodeIndex];
		int32 Result = -1;
		double LastDot = -1;

		for (const int32 OtherIndex : Node.AdjacentNodes)
		{
			if (Nodes[OtherIndex].AdjacentNodes.Num() < MinNeighborCount) { continue; }
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

	int32 FClusterProjection::FindNextAdjacentNode(const EPCGExClusterSearchOrientationMode Orient, const int32 NodeIndex, const int32 From, const TSet<int32>& Exclusion, const int32 MinNeighbors)
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

	PCGExDataFilter::EType TClusterFilter::GetFilterType() const { return PCGExDataFilter::EType::Cluster; }

	void TClusterFilter::CaptureCluster(const FPCGContext* InContext, const FCluster* InCluster)
	{
		bValid = true;
		CapturedCluster = InCluster;

		Capture(InContext, InCluster->PointsIO);
		if (bValid) { CaptureEdges(InContext, InCluster->EdgesIO); } // Only capture edges if we could capture nodes
	}

	void TClusterFilter::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
	{
		// Do not call Super:: as it sets bValid to true, and we want CaptureCluster to have control
		// This is the filter is invalid if used somewhere that doesn't initialize clusters.
		//TFilterHandler::Capture(PointIO);
	}

	void TClusterFilter::CaptureEdges(const FPCGContext* InContext, const PCGExData::FPointIO* EdgeIO)
	{
	}

	void TClusterFilter::PrepareForTesting(const PCGExData::FPointIO* PointIO)
	{
		const int32 NumNodes = CapturedCluster->Nodes.Num();
		Results.SetNumUninitialized(NumNodes);
		for (int i = 0; i < NumNodes; i++) { Results[i] = false; }
	}

	FNodeStateHandler::FNodeStateHandler(const UPCGExNodeStateFactory* InFactory)
		: TDataState(InFactory), NodeStateDefinition(InFactory)
	{
		const int32 NumConditions = InFactory->FilterFactories.Num();

		FilterHandlers.Empty();
		ClusterFilterHandlers.Empty();

		for (int i = 0; i < NumConditions; i++)
		{
			if (TFilter* Handler = InFactory->FilterFactories[i]->CreateFilter())
			{
				if (Handler->GetFilterType() == PCGExDataFilter::EType::Cluster) { ClusterFilterHandlers.Add(static_cast<TClusterFilter*>(Handler)); }
				else { FilterHandlers.Add(Handler); }
			}
			else
			{
				//Exception catch
			}
		}
	}

	void FNodeStateHandler::CaptureCluster(const FPCGContext* InContext, FCluster* InCluster)
	{
		Cluster = InCluster;
		TArray<TFilter*> InvalidTests;
		TArray<TClusterFilter*> InvalidClusterTests;

		for (TFilter* Test : FilterHandlers)
		{
			Test->Capture(InContext, Cluster->PointsIO);
			if (!Test->bValid) { InvalidTests.Add(Test); }
		}

		for (TFilter* Filter : InvalidTests)
		{
			FilterHandlers.Remove(Filter);
			delete Filter;
		}

		for (TClusterFilter* Test : ClusterFilterHandlers)
		{
			Test->CaptureCluster(InContext, Cluster);
			if (!Test->bValid) { InvalidClusterTests.Add(Test); }
		}

		for (TClusterFilter* Filter : InvalidClusterTests)
		{
			ClusterFilterHandlers.Remove(Filter);
			delete Filter;
		}

		InvalidTests.Empty();
		InvalidClusterTests.Empty();

		bValid = !FilterHandlers.IsEmpty() || !ClusterFilterHandlers.IsEmpty();
	}

	bool FNodeStateHandler::Test(const int32 PointIndex) const
	{
		if (!FilterHandlers.IsEmpty())
		{
			for (const TFilter* Test : FilterHandlers) { if (!Test->Test(PointIndex)) { return false; } }
		}

		if (!ClusterFilterHandlers.IsEmpty())
		{
			const int32 NodeIndex = Cluster->GetNodeFromPointIndex(PointIndex).NodeIndex; // We get a point index from the FindNode
			for (const TClusterFilter* Test : ClusterFilterHandlers) { if (!Test->Test(NodeIndex)) { return false; } }
		}

		return true;
	}

	void FNodeStateHandler::PrepareForTesting(const PCGExData::FPointIO* PointIO)
	{
		TDataState::PrepareForTesting(PointIO);
		for (TFilter* Test : FilterHandlers) { Test->PrepareForTesting(PointIO); }
		for (TClusterFilter* Test : ClusterFilterHandlers) { Test->PrepareForTesting(PointIO); }
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
