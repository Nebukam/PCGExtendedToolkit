// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCluster.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Geometry/PCGExGeo.h"

#pragma region UPCGExNodeStateDefinition

PCGExFactories::EType UPCGExClusterFilterFactoryBase::GetFactoryType() const
{
	return PCGExFactories::EType::ClusterFilter;
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
		Adjacency.Empty();
	}

	bool FNode::IsAdjacentTo(const int32 OtherNodeIndex) const
	{
		for (const uint64 AdjacencyHash : Adjacency) { if (OtherNodeIndex == PCGEx::H64A(AdjacencyHash)) { return true; } }
		return false;
	}

	void FNode::AddConnection(const int32 InNodeIndex, const int32 InEdgeIndex)
	{
		Adjacency.AddUnique(PCGEx::H64(InNodeIndex, InEdgeIndex));
	}

	FVector FNode::GetCentroid(FCluster* InCluster) const
	{
		if (Adjacency.IsEmpty()) { return Position; }

		FVector Centroid = FVector::ZeroVector;
		const int32 NumPoints = Adjacency.Num();

		for (int i = 0; i < NumPoints; i++) { Centroid += InCluster->Nodes[PCGEx::H64A(Adjacency[i])].Position; }

		if (Adjacency.Num() < 2)
		{
			Centroid += Position;
			return Centroid / 2;
		}

		Centroid /= static_cast<double>(NumPoints);

		return Centroid;
	}

	int32 FNode::GetEdgeIndex(const int32 AdjacentNodeIndex) const
	{
		for (const uint64 AdjacencyHash : Adjacency)
		{
			uint32 NIndex;
			uint32 EIndex;
			PCGEx::H64(AdjacencyHash, NIndex, EIndex);
			if (NIndex == AdjacentNodeIndex) { return EIndex; }
		}
		return -1;
	}

#pragma endregion

#pragma region FCluster

	FCluster::FCluster()
	{
		NodeIndexLookup.Empty();
		Nodes.Empty();
		Edges.Empty();
		Bounds = FBox(ForceInit);
	}

	FCluster::~FCluster()
	{
		Nodes.Empty();
		NodeIndexLookup.Empty();
		Edges.Empty();
		EdgeLengths.Empty();

		PCGEX_DELETE(NodeOctree)
		PCGEX_DELETE(EdgeOctree)
	}

	bool FCluster::BuildFrom(
		const PCGExData::FPointIO& EdgeIO,
		const TArray<FPCGPoint>& InNodePoints,
		const TMap<int64, int32>& InEndpointsLookup,
		const TArray<int32>* InExpectedAdjacency)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCluster::BuildCluster);

		Nodes.Empty();
		Edges.Empty();
		NodeIndexLookup.Empty();
		//EdgeIndexLookup.Empty();

		TSet<int32> NodePointsSet;

		if (!BuildIndexedEdges(EdgeIO, InEndpointsLookup, Edges, NodePointsSet, true)) { return false; }

		bool bInvalidCluster = false;

		const int32 NumNodes = NodePointsSet.Num();
		Nodes.SetNum(NumNodes);
		NodeIndexLookup.Reserve(NumNodes);

		int32 NodeIndex = 0;
		for (int32 PointIndex : NodePointsSet)
		{
			Nodes[NodeIndex] = PCGExCluster::FNode(NodeIndex, PointIndex, InNodePoints[PointIndex].Transform.GetLocation());
			NodeIndexLookup.Add(PointIndex, NodeIndex);
			NodeIndex++;
		}

		const int32 NumEdges = Edges.Num();
		//EdgeIndexLookup.Reserve(NumEdges);

		/*
		EdgeList.Sort(
			[](const PCGExGraph::FIndexedEdge& A, const PCGExGraph::FIndexedEdge& B)
			{
				return A.Start == B.Start ? A.End < B.End : A.Start < B.Start;
			});
			*/

		for (int i = 0; i < NumEdges; i++)
		{
			const PCGExGraph::FIndexedEdge& SortedEdge = Edges[i];
			//SortedEdge.EdgeIndex = i; // Only required if we sort the array first

			const int32 StartNodeIndex = *NodeIndexLookup.Find(SortedEdge.Start);
			const int32 EndNodeIndex = *NodeIndexLookup.Find(SortedEdge.End);

			//EdgeIndexLookup.Add(PCGEx::H64U(StartNodeIndex, EndNodeIndex), i);

			Nodes[StartNodeIndex].Adjacency.AddUnique(PCGEx::H64(EndNodeIndex, i));
			Nodes[EndNodeIndex].Adjacency.AddUnique(PCGEx::H64(StartNodeIndex, i));
		}

		if (InExpectedAdjacency)
		{
			for (const FNode& Node : Nodes)
			{
				if ((*InExpectedAdjacency)[Node.PointIndex] > Node.Adjacency.Num()) // We care about removed connections, not new ones 
				{
					bInvalidCluster = true;
					break;
				}
			}
		}

		bValid = !bInvalidCluster;
		return bValid;
	}

	void FCluster::RebuildNodeOctree()
	{
		PCGEX_DELETE(NodeOctree)
		NodeOctree = new ClusterItemOctree(Bounds.GetCenter(), Bounds.GetExtent().Length());
		for (const FNode& Node : Nodes) { NodeOctree->AddElement(FClusterItemRef(Node.NodeIndex, FBoxSphereBounds(FSphere(Node.Position, 0)))); }
	}

	void FCluster::RebuildEdgeOctree()
	{
		PCGEX_DELETE(EdgeOctree)
		EdgeOctree = new ClusterItemOctree(Bounds.GetCenter(), Bounds.GetExtent().Length());
		for (const PCGExGraph::FIndexedEdge& Edge : Edges)
		{
			const FNode& Start = Nodes[*NodeIndexLookup.Find(Edge.Start)];
			const FNode& End = Nodes[*NodeIndexLookup.Find(Edge.End)];
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
				if (Node.Adjacency.Num() < MinNeighbors) { return; }
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
				if (Node.Adjacency.Num() < MinNeighbors) { continue; }
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
				const FNode& Start = Nodes[*NodeIndexLookup.Find(Edge.Start)];
				const FNode& End = Nodes[*NodeIndexLookup.Find(Edge.End)];
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
				const FNode& Start = Nodes[*NodeIndexLookup.Find(Edge.Start)];
				const FNode& End = Nodes[*NodeIndexLookup.Find(Edge.End)];
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
		const FNode& Start = Nodes[*NodeIndexLookup.Find(Edge.Start)];
		const FNode& End = Nodes[*NodeIndexLookup.Find(Edge.End)];

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
				if (Nodes[Item.ItemIndex].Adjacency.Num() < MinNeighborCount) { return; }
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
			for (const int32 OtherIndex : Node.Adjacency)
			{
				if (Nodes[OtherIndex].Adjacency.Num() < MinNeighborCount) { continue; }
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
				if (Nodes[Item.ItemIndex].Adjacency.Num() < MinNeighborCount) { return; }
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
			for (const int32 OtherIndex : Node.Adjacency)
			{
				if (Nodes[OtherIndex].Adjacency.Num() < MinNeighborCount) { continue; }
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
			const double Dist = FVector::DistSquared(PointsIO->GetInPoint(Edge.Start).Transform.GetLocation(), PointsIO->GetInPoint(Edge.End).Transform.GetLocation());
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

		for (const uint64 AdjacencyHash : RootNode.Adjacency)
		{
			const int32 AdjacentIndex = PCGEx::H64A(AdjacencyHash);
			if (OutIndices.Contains(AdjacentIndex)) { continue; }

			OutIndices.Add(AdjacentIndex);
			if (NextDepth > 0) { GetConnectedNodes(AdjacentIndex, OutIndices, NextDepth); }
		}
	}

	void FCluster::GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth, const TSet<int32>& Skip) const
	{
		const int32 NextDepth = SearchDepth - 1;
		const FNode& RootNode = Nodes[FromIndex];

		for (const uint64 AdjacencyHash : RootNode.Adjacency)
		{
			const int32 AdjacentIndex = PCGEx::H64A(AdjacencyHash);
			if (Skip.Contains(AdjacentIndex) || OutIndices.Contains(AdjacentIndex)) { continue; }

			OutIndices.Add(AdjacentIndex);
			if (NextDepth > 0) { GetConnectedNodes(AdjacentIndex, OutIndices, NextDepth, Skip); }
		}
	}

	void FCluster::GetConnectedEdges(const int32 FromNodeIndex, TArray<int32>& OutNodeIndices, TArray<int32>& OutEdgeIndices, const int32 SearchDepth) const
	{
		const int32 NextDepth = SearchDepth - 1;
		const FNode& RootNode = Nodes[FromNodeIndex];

		for (const uint64 AdjacencyHash : RootNode.Adjacency)
		{
			uint32 AdjacentIndex;
			uint32 EdgeIndex;
			PCGEx::H64(AdjacencyHash, AdjacentIndex, EdgeIndex);

			if (OutNodeIndices.Contains(AdjacentIndex)) { continue; }
			if (OutEdgeIndices.Contains(EdgeIndex)) { continue; }

			OutNodeIndices.Add(AdjacentIndex);
			OutEdgeIndices.Add(EdgeIndex);

			if (NextDepth > 0) { GetConnectedEdges(AdjacentIndex, OutNodeIndices, OutEdgeIndices, NextDepth); }
		}
	}

	void FCluster::GetConnectedEdges(const int32 FromNodeIndex, TArray<int32>& OutNodeIndices, TArray<int32>& OutEdgeIndices, const int32 SearchDepth, const TSet<int32>& SkipNodes, const TSet<int32>& SkipEdges) const
	{
		const int32 NextDepth = SearchDepth - 1;
		const FNode& RootNode = Nodes[FromNodeIndex];

		for (const uint64 AdjacencyHash : RootNode.Adjacency)
		{
			uint32 AdjacentIndex;
			uint32 EdgeIndex;
			PCGEx::H64(AdjacencyHash, AdjacentIndex, EdgeIndex);

			if (SkipNodes.Contains(AdjacentIndex) || OutNodeIndices.Contains(AdjacentIndex)) { continue; }
			if (SkipEdges.Contains(EdgeIndex) || OutEdgeIndices.Contains(EdgeIndex)) { continue; }

			OutNodeIndices.Add(AdjacentIndex);
			OutEdgeIndices.Add(EdgeIndex);

			if (NextDepth > 0) { GetConnectedEdges(AdjacentIndex, OutNodeIndices, OutEdgeIndices, NextDepth, SkipNodes, SkipEdges); }
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
		for (const uint64 AdjacencyHash : Node.Adjacency) { Centroid += Nodes[PCGEx::H64A(AdjacencyHash)].Position; }
		return Centroid / static_cast<double>(Node.Adjacency.Num());
	}

	int32 FCluster::FindClosestNeighborInDirection(const int32 NodeIndex, const FVector& Direction, const int32 MinNeighborCount) const
	{
		const FNode& Node = Nodes[NodeIndex];
		int32 Result = -1;
		double LastDot = -1;

		for (const uint64 AdjacencyHash : Node.Adjacency)
		{
			const int32 AdjacentIndex = PCGEx::H64A(AdjacencyHash);
			if (Nodes[AdjacentIndex].Adjacency.Num() < MinNeighborCount) { continue; }
			if (const double Dot = FVector::DotProduct(Direction, (Node.Position - Nodes[AdjacentIndex].Position).GetSafeNormal());
				Dot > LastDot)
			{
				LastDot = Dot;
				Result = AdjacentIndex;
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

		const int32 NumNodes = Node->Adjacency.Num();
		SortedAdjacency.SetNum(NumNodes);

		TArray<int32> Sort;
		TArray<double> Angles;

		Sort.SetNum(NumNodes);
		Angles.SetNum(NumNodes);

		for (int i = 0; i < NumNodes; i++)
		{
			Sort[i] = i;
			FVector Direction = ProjectionSettings->Project((Node->Position - InCluster->Nodes[PCGEx::H64A(Node->Adjacency[i])].Position), Node->PointIndex);
			Direction.Z = 0;
			Angles[i] = PCGExMath::GetAngle(FVector::ForwardVector, Direction.GetSafeNormal());
		}

		Sort.Sort([&](const int32& A, const int32& B) { return Angles[A] > Angles[B]; });
		for (int i = 0; i < NumNodes; i++) { SortedAdjacency[i] = Node->Adjacency[Sort[i]]; }

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

		Normal = PCGExMath::GetNormal(InCluster->Nodes[PCGEx::H64A(SortedAdjacency.Last())].Position, Node->Position, InCluster->Nodes[PCGEx::H64A(SortedAdjacency[0])].Position);

		if (SortedAdjacency.Num() < 2) { return; }

		for (int i = 0; i < SortedAdjacency.Num() - 1; i++)
		{
			Normal += PCGExMath::GetNormal(InCluster->Nodes[PCGEx::H64A(SortedAdjacency[i])].Position, Node->Position, InCluster->Nodes[PCGEx::H64A(SortedAdjacency[i + 1])].Position);
		}

		Normal /= SortedAdjacency.Num();
	}

	int32 FNodeProjection::GetAdjacencyIndex(const int32 NodeIndex) const
	{
		for (int i = 0; i < SortedAdjacency.Num(); i++) { if (PCGEx::H64A(SortedAdjacency[i]) == NodeIndex) { return i; } }
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
			const int32 NextIndex = PCGEx::H64A(Project.SortedAdjacency[PCGExMath::Tile(StartIndex + i + 1, 0, NumNodes - 1)]);
			if ((NextIndex == From && NumNodes > 1) ||
				Exclusion.Contains(NextIndex) ||
				Cluster->Nodes[NextIndex].Adjacency.Num() < MinNeighbors) { continue; }

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
			const int32 NextIndex = PCGEx::H64A(Project.SortedAdjacency[PCGExMath::Tile(StartIndex - i - 1, 0, NumNodes - 1)]);
			if ((NextIndex == From && NumNodes > 1) ||
				Exclusion.Contains(NextIndex) ||
				Cluster->Nodes[NextIndex].Adjacency.Num() < MinNeighbors) { continue; }

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

	bool TClusterFilter::PrepareForTesting(const PCGExData::FPointIO* PointIO)
	{
		if (bCacheResults)
		{
			const int32 NumNodes = CapturedCluster->Nodes.Num();
			Results.SetNumUninitialized(NumNodes);
			for (int i = 0; i < NumNodes; i++) { Results[i] = false; }
		}

		return false;
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
			const int32 NodeIndex = *Cluster->NodeIndexLookup.Find(PointIndex); // We get a point index from the FindNode
			for (const TClusterFilter* Test : ClusterFilterHandlers) { if (!Test->Test(NodeIndex)) { return false; } }
		}

		return true;
	}

	bool FNodeStateHandler::PrepareForTesting(const PCGExData::FPointIO* PointIO)
	{
		TDataState::PrepareForTesting(PointIO);

		HeavyFilterHandlers.Empty();
		HeavyClusterFilterHandlers.Empty();

		for (TFilter* Test : FilterHandlers) { if (Test->PrepareForTesting(PointIO)) { HeavyFilterHandlers.Add(Test); } }
		for (TClusterFilter* Test : ClusterFilterHandlers) { if (Test->PrepareForTesting(PointIO)) { HeavyClusterFilterHandlers.Add(Test); } }

		return RequiresPerPointPreparation();
	}

	void FNodeStateHandler::PrepareSingle(const int32 PointIndex)
	{
		for (TFilter* Test : HeavyFilterHandlers) { Test->PrepareSingle(PointIndex); }

		if (!HeavyClusterFilterHandlers.IsEmpty())
		{
			const int32 NodeIndex = *Cluster->NodeIndexLookup.Find(PointIndex); // We get a point index from the FindNode
			for (TClusterFilter* Test : HeavyClusterFilterHandlers) { Test->PrepareSingle(NodeIndex); }
		}
	}

	void FNodeStateHandler::PreparationComplete()
	{
		for (TFilter* Test : HeavyFilterHandlers) { Test->PreparationComplete(); }
		for (TClusterFilter* Test : HeavyClusterFilterHandlers) { Test->PreparationComplete(); }
	}

	bool FNodeStateHandler::RequiresPerPointPreparation() const
	{
		return !HeavyFilterHandlers.IsEmpty() || !HeavyClusterFilterHandlers.IsEmpty();;
	}

#pragma endregion
}

namespace PCGExClusterTask
{
	bool FBuildCluster::ExecuteTask()
	{
		Cluster->BuildFrom(
			*EdgeIO, PointIO->GetIn()->GetPoints(),
			*EndpointsLookup, ExpectedAdjacency);

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

	FCopyClustersToPoint::~FCopyClustersToPoint()
	{
		Edges.Empty();
	}

	bool FCopyClustersToPoint::ExecuteTask()
	{
		PCGExData::FPointIO& VtxDupe = VtxCollection->Emplace_GetRef(Vtx->GetIn(), PCGExData::EInit::DuplicateInput);
		VtxDupe.IOIndex = TaskIndex;

		FString OutId;
		VtxDupe.Tags->Set(PCGExGraph::TagStr_ClusterPair, VtxDupe.GetOut()->UID, OutId);

		Manager->Start<PCGExGeoTasks::FTransformPointIO>(TaskIndex, PointIO, &VtxDupe, TransformSettings);

		for (const PCGExData::FPointIO* EdgeIO : Edges)
		{
			PCGExData::FPointIO& EdgeDupe = EdgeCollection->Emplace_GetRef(EdgeIO->GetIn(), PCGExData::EInit::DuplicateInput);
			EdgeDupe.IOIndex = TaskIndex;
			EdgeDupe.Tags->Set(PCGExGraph::TagStr_ClusterPair, OutId);

			Manager->Start<PCGExGeoTasks::FTransformPointIO>(TaskIndex, PointIO, &EdgeDupe, TransformSettings);
		}

		return true;
	}
}
