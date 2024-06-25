// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCluster.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Geometry/PCGExGeo.h"

#pragma region UPCGExNodeStateDefinition

PCGExFactories::EType UPCGExClusterFilterFactoryBase::GetFactoryType() const
{
	return PCGExFactories::EType::ClusterNodeFilter;
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

	bool FNode::IsDeadEnd() const { return Adjacency.Num() == 1; }
	bool FNode::IsSimple() const { return Adjacency.Num() == 2; }
	bool FNode::IsComplex() const { return Adjacency.Num() > 2; }

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

		TArray<FNode>& Nodes = *InCluster->Nodes;
		for (int i = 0; i < NumPoints; i++) { Centroid += Nodes[PCGEx::H64A(Adjacency[i])].Position; }

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

	void FNode::ExtractAdjacencies(TArray<int32>& OutNodes, TArray<int32>& OutEdges) const
	{
		const int32 NumAdjacency = Adjacency.Num();
		OutNodes.SetNumUninitialized(NumAdjacency);
		OutEdges.SetNumUninitialized(NumAdjacency);

		for (int i = 0; i < NumAdjacency; i++)
		{
			uint32 AdjacentNode;
			uint32 AdjacentEdge;
			PCGEx::H64(Adjacency[i], AdjacentNode, AdjacentEdge);
			OutNodes[i] = AdjacentNode;
			OutEdges[i] = AdjacentEdge;
		}
	}

	void FNode::Add(const FNode& Neighbor, int32 EdgeIndex)
	{
		Adjacency.Add(PCGEx::H64(Neighbor.NodeIndex, EdgeIndex));
	}

#pragma endregion

#pragma region FCluster

	FCluster::FCluster()
	{
		NodeIndexLookup = new TMap<int32, int32>();
		Nodes = new TArray<FNode>();
		Edges = new TArray<PCGExGraph::FIndexedEdge>();
		Bounds = FBox(ForceInit);
	}

	FCluster::FCluster(const FCluster* OtherCluster, PCGExData::FPointIO* InVtxIO, PCGExData::FPointIO* InEdgesIO,
	                   bool bCopyNodes, bool bCopyEdges, bool bCopyLookup)
	{
		bIsMirror = true;
		bIsCopyCluster = false;

		VtxIO = InVtxIO;
		EdgesIO = InEdgesIO;

		NumRawVtx = InVtxIO->GetNum();
		NumRawEdges = InEdgesIO->GetNum();

		if (bCopyNodes)
		{
			Nodes = new TArray<FNode>();
			Nodes->Reserve(OtherCluster->Nodes->Num());
			Nodes->Append(*OtherCluster->Nodes);
		}
		else
		{
			bOwnsNodes = false;
			Nodes = OtherCluster->Nodes;
		}

		if (bCopyEdges)
		{
			Edges = new TArray<PCGExGraph::FIndexedEdge>();
			Edges->Reserve(OtherCluster->Edges->Num());
			Edges->Append(*OtherCluster->Edges);
		}
		else
		{
			Edges = OtherCluster->Edges;
			bOwnsEdges = false;
		}

		if (bCopyLookup)
		{
			NodeIndexLookup = new TMap<int32, int32>();
			NodeIndexLookup->Append(*OtherCluster->NodeIndexLookup);
		}
		else
		{
			NodeIndexLookup = OtherCluster->NodeIndexLookup;
			bOwnsNodeIndexLookup = false;
		}

		Bounds = OtherCluster->Bounds;

		EdgeLengths = OtherCluster->EdgeLengths;
		if (EdgeLengths)
		{
			bEdgeLengthsDirty = false;
			bOwnsLengths = false;
		}

		VtxPointIndices = OtherCluster->VtxPointIndices;
		if (VtxPointIndices) { bOwnsVtxPointIndices = false; }

		NodeOctree = OtherCluster->NodeOctree;
		if (NodeOctree) { bOwnsNodeOctree = false; }

		EdgeOctree = OtherCluster->EdgeOctree;
		if (EdgeOctree) { bOwnsEdgeOctree = false; }
	}

	FCluster::~FCluster()
	{
		if (bOwnsNodeIndexLookup) { PCGEX_DELETE(NodeIndexLookup) }
		if (bOwnsNodes) { PCGEX_DELETE(Nodes) }
		if (bOwnsEdges) { PCGEX_DELETE(Edges) }
		if (bOwnsLengths) { PCGEX_DELETE(EdgeLengths) }
		if (bOwnsVtxPointIndices) { PCGEX_DELETE(VtxPointIndices) }
		if (bOwnsNodeOctree) { PCGEX_DELETE(NodeOctree) }
		if (bOwnsEdgeOctree) { PCGEX_DELETE(EdgeOctree) }
	}

	bool FCluster::BuildFrom(
		const PCGExData::FPointIO* EdgeIO,
		const TArray<FPCGPoint>& InNodePoints,
		const TMap<uint32, int32>& InEndpointsLookup,
		const TArray<int32>* InExpectedAdjacency)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCluster::BuildCluster);

		Nodes->Empty();
		Edges->Empty();
		NodeIndexLookup->Empty();

		NumRawVtx = InNodePoints.Num();
		NumRawEdges = EdgeIO->GetNum();

		PCGEx::TFAttributeReader<int64>* EndpointsReader = new PCGEx::TFAttributeReader<int64>(PCGExGraph::Tag_EdgeEndpoints);

		auto OnFail = [&]()
		{
			Nodes->Empty();
			Edges->Empty();
			PCGEX_DELETE(EndpointsReader)
			return false;
		};

		if (!EndpointsReader->Bind(const_cast<PCGExData::FPointIO*>(EdgeIO))) { return OnFail(); }

		const int32 NumEdges = EdgeIO->GetNum();

		Edges->SetNumUninitialized(NumEdges);
		Nodes->Reserve(InNodePoints.Num());
		NodeIndexLookup->Reserve(InNodePoints.Num());

		for (int i = 0; i < NumEdges; i++)
		{
			uint32 A;
			uint32 B;
			PCGEx::H64(EndpointsReader->Values[i], A, B);

			const int32* StartPointIndexPtr = InEndpointsLookup.Find(A);
			const int32* EndPointIndexPtr = InEndpointsLookup.Find(B);

			if ((!StartPointIndexPtr || !EndPointIndexPtr)) { return OnFail(); }

			FNode& StartNode = GetOrCreateNodeUnsafe(InNodePoints, *StartPointIndexPtr);
			FNode& EndNode = GetOrCreateNodeUnsafe(InNodePoints, *EndPointIndexPtr);

			StartNode.Add(EndNode, i);
			EndNode.Add(StartNode, i);

			(*Edges)[i] = PCGExGraph::FIndexedEdge(i, *StartPointIndexPtr, *EndPointIndexPtr, i, EdgeIO->IOIndex);
		}

		if (InExpectedAdjacency)
		{
			for (const FNode& Node : (*Nodes))
			{
				if ((*InExpectedAdjacency)[Node.PointIndex] > Node.Adjacency.Num()) // We care about removed connections, not new ones 
				{
					return OnFail();
				}
			}
		}

		NodeIndexLookup->Shrink();
		Nodes->Shrink();

		return true;
	}

	void FCluster::BuildFrom(const PCGExGraph::FSubGraph* SubGraph)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCluster::BuildClusterFromSubgraph);

		NumRawVtx = SubGraph->VtxIO->GetOutNum();
		NumRawEdges = SubGraph->EdgesIO->GetOutNum();

		const TArray<FPCGPoint>& VtxPoints = SubGraph->VtxIO->GetOutIn()->GetPoints();
		Nodes->Reserve(SubGraph->Nodes.Num());

		TArray<PCGExGraph::FIndexedEdge>& EdgesRef = *Edges;
		EdgesRef.Reserve(NumRawEdges);
		EdgesRef.Append(SubGraph->FlattenedEdges);

		// TODO : This can probably be more optimal

		for (const PCGExGraph::FIndexedEdge& E : SubGraph->FlattenedEdges)
		{
			FNode& StartNode = GetOrCreateNodeUnsafe(VtxPoints, E.Start);
			FNode& EndNode = GetOrCreateNodeUnsafe(VtxPoints, E.End);

			StartNode.Add(EndNode, E.EdgeIndex);
			EndNode.Add(StartNode, E.EdgeIndex);
		}
	}

	bool FCluster::IsValidWith(const PCGExData::FPointIO* InVtxIO, const PCGExData::FPointIO* InEdgesIO) const
	{
		return NumRawVtx == InVtxIO->GetNum() && NumRawEdges == InEdgesIO->GetNum();
	}

	const TArray<int32>* FCluster::GetVtxPointIndicesPtr()
	{
		{
			FReadScopeLock ReadScopeLock(ClusterLock);
			if (VtxPointIndices) { return VtxPointIndices; }
		}

		CreateVtxPointIndices();
		return VtxPointIndices;
	}

	const TArray<int32>& FCluster::GetVtxPointIndices()
	{
		return *GetVtxPointIndicesPtr();
	}

	TArrayView<const int32> FCluster::GetVtxPointIndicesView()
	{
		GetVtxPointIndicesPtr();
		return MakeArrayView(VtxPointIndices->GetData(), VtxPointIndices->Num());
	}


	const TArray<uint64>* FCluster::GetVtxPointScopesPtr()
	{
		{
			FReadScopeLock ReadScopeLock(ClusterLock);
			if (VtxPointScopes) { return VtxPointScopes; }
		}

		CreateVtxPointScopes();
		return VtxPointScopes;
	}

	const TArray<uint64>& FCluster::GetVtxPointScopes()
	{
		return *GetVtxPointScopesPtr();
	}

	TArrayView<const uint64> FCluster::GetVtxPointScopesView()
	{
		GetVtxPointScopesPtr();
		return MakeArrayView(VtxPointScopes->GetData(), VtxPointScopes->Num());
	}


	void FCluster::RebuildBounds()
	{
		for (const FNode& Node : (*Nodes)) { Bounds += Node.Position; }
	}

	void FCluster::RebuildNodeOctree()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCluster::RebuildNodeOctree);
		PCGEX_DELETE(NodeOctree)
		const TArray<FPCGPoint> VtxPoints = VtxIO->GetIn()->GetPoints();
		NodeOctree = new ClusterItemOctree(Bounds.GetCenter(), (Bounds.GetExtent() + FVector(10)).Length());
		for (const FNode& Node : (*Nodes))
		{
			const FPCGPoint& Pt = VtxPoints[Node.PointIndex];
			NodeOctree->AddElement(FClusterItemRef(Node.NodeIndex, FBoxSphereBounds(Pt.GetLocalBounds().TransformBy(Pt.Transform))));
		}
	}

	void FCluster::RebuildEdgeOctree()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCluster::RebuildEdgeOctree);
		PCGEX_DELETE(EdgeOctree)

		const TArray<FNode>& NodesRef = *Nodes;
		const TMap<int32, int32>& NodeIndexLookupRef = *NodeIndexLookup;

		EdgeOctree = new ClusterItemOctree(Bounds.GetCenter(), (Bounds.GetExtent() + FVector(10)).Length());
		for (const PCGExGraph::FIndexedEdge& Edge : (*Edges))
		{
			const FNode& Start = NodesRef[NodeIndexLookupRef[Edge.Start]];
			const FNode& End = NodesRef[NodeIndexLookupRef[Edge.End]];
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

		const TArray<FNode>& NodesRef = *Nodes;

		if (NodeOctree)
		{
			auto ProcessCandidate = [&](const FClusterItemRef& Item)
			{
				const FNode& Node = NodesRef[Item.ItemIndex];
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
			for (const FNode& Node : (*Nodes))
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

		const TArray<FNode>& NodesRef = *Nodes;
		const TArray<PCGExGraph::FIndexedEdge>& EdgesRef = *Edges;
		const TMap<int32, int32>& NodeIndexLookupRef = *NodeIndexLookup;

		if (EdgeOctree)
		{
			auto ProcessCandidate = [&](const FClusterItemRef& Item)
			{
				const PCGExGraph::FIndexedEdge& Edge = EdgesRef[Item.ItemIndex];
				const FNode& Start = NodesRef[NodeIndexLookupRef[Edge.Start]];
				const FNode& End = NodesRef[NodeIndexLookupRef[Edge.End]];
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
			for (const PCGExGraph::FIndexedEdge& Edge : (*Edges))
			{
				const FNode& Start = NodesRef[NodeIndexLookupRef[Edge.Start]];
				const FNode& End = NodesRef[NodeIndexLookupRef[Edge.End]];
				const double Dist = FMath::PointDistToSegmentSquared(Position, Start.Position, End.Position);
				if (Dist < MaxDistance)
				{
					MaxDistance = Dist;
					ClosestIndex = Edge.EdgeIndex;
				}
			}
		}

		if (ClosestIndex == -1) { return -1; }

		const PCGExGraph::FIndexedEdge& Edge = EdgesRef[ClosestIndex];
		const FNode& Start = NodesRef[NodeIndexLookupRef[Edge.Start]];
		const FNode& End = NodesRef[NodeIndexLookupRef[Edge.End]];

		ClosestIndex = FVector::DistSquared(Position, Start.Position) < FVector::DistSquared(Position, End.Position) ? Start.NodeIndex : End.NodeIndex;

		return ClosestIndex;
	}

	int32 FCluster::FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, const int32 MinNeighborCount) const
	{
		const TArray<FNode>& NodesRef = *Nodes;
		const FNode& Node = NodesRef[NodeIndex];
		int32 Result = -1;
		double LastDist = TNumericLimits<double>::Max();

		if (NodeOctree)
		{
			auto ProcessCandidate = [&](const FClusterItemRef& Item)
			{
				if (NodesRef[Item.ItemIndex].Adjacency.Num() < MinNeighborCount) { return; }
				if (const double Dist = FMath::PointDistToSegmentSquared(Position, Node.Position, NodesRef[Item.ItemIndex].Position);
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
				if (NodesRef[OtherIndex].Adjacency.Num() < MinNeighborCount) { continue; }
				if (const double Dist = FMath::PointDistToSegmentSquared(Position, Node.Position, NodesRef[OtherIndex].Position);
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
		const TArray<FNode>& NodesRef = *Nodes;
		const FNode& Node = NodesRef[NodeIndex];
		int32 Result = -1;
		double LastDist = TNumericLimits<double>::Max();

		if (NodeOctree)
		{
			auto ProcessCandidate = [&](const FClusterItemRef& Item)
			{
				if (NodesRef[Item.ItemIndex].Adjacency.Num() < MinNeighborCount) { return; }
				if (Exclusion.Contains(Item.ItemIndex)) { return; }
				if (const double Dist = FMath::PointDistToSegmentSquared(Position, Node.Position, NodesRef[Item.ItemIndex].Position);
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
				if (NodesRef[OtherIndex].Adjacency.Num() < MinNeighborCount) { continue; }
				if (Exclusion.Contains(OtherIndex)) { continue; }
				if (const double Dist = FMath::PointDistToSegmentSquared(Position, Node.Position, NodesRef[OtherIndex].Position);
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
		if (EdgeLengths) { return; }

		EdgeLengths = new TArray<double>();
		TArray<double>& LengthsRef = *EdgeLengths;

		const TArray<FNode>& NodesRef = *Nodes;
		const TArray<PCGExGraph::FIndexedEdge>& EdgesRef = *Edges;
		const TMap<int32, int32>& NodeIndexLookupRef = *NodeIndexLookup;

		const int32 NumEdges = Edges->Num();
		double Min = TNumericLimits<double>::Max();
		double Max = TNumericLimits<double>::Min();
		EdgeLengths->SetNumUninitialized(NumEdges);

		for (int i = 0; i < NumEdges; i++)
		{
			const PCGExGraph::FIndexedEdge& Edge = EdgesRef[i];
			const double Dist = FVector::DistSquared(NodesRef[NodeIndexLookupRef[Edge.Start]].Position, NodesRef[NodeIndexLookupRef[Edge.End]].Position);
			LengthsRef[i] = Dist;
			Min = FMath::Min(Dist, Min);
			Max = FMath::Max(Dist, Max);
		}

		//Normalized to 0 instead of min
		if (bNormalize) { for (int i = 0; i < NumEdges; i++) { LengthsRef[i] = PCGExMath::Remap(LengthsRef[i], 0, Max, 0, 1); } }

		bEdgeLengthsDirty = false;
	}

	void FCluster::GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth) const
	{
		const int32 NextDepth = SearchDepth - 1;
		const FNode& RootNode = (*Nodes)[FromIndex];

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
		const FNode& RootNode = (*Nodes)[FromIndex];

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
		const FNode& RootNode = (*Nodes)[FromNodeIndex];

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
		const FNode& RootNode = (*Nodes)[FromNodeIndex];

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
		return ((*Nodes)[FromIndex].Position - (*Nodes)[ToIndex].Position).GetSafeNormal();
	}

	FVector FCluster::GetCentroid(const int32 NodeIndex) const
	{
		const TArray<FNode>& NodesRef = *Nodes;

		const FNode& Node = NodesRef[NodeIndex];
		FVector Centroid = FVector::ZeroVector;
		for (const uint64 AdjacencyHash : Node.Adjacency) { Centroid += NodesRef[PCGEx::H64A(AdjacencyHash)].Position; }
		return Centroid / static_cast<double>(Node.Adjacency.Num());
	}

	void FCluster::GetValidEdges(TArray<PCGExGraph::FIndexedEdge>& OutValidEdges) const
	{
		TArray<FNode>& NodesRef = (*Nodes);
		TMap<int32, int32>& LookupRef = (*NodeIndexLookup);
		for (const PCGExGraph::FIndexedEdge& Edge : (*Edges))
		{
			if (!Edge.bValid ||
				!NodesRef[LookupRef[Edge.Start]].bValid || // Adds quite the cost
				!NodesRef[LookupRef[Edge.End]].bValid)
			{
				continue;
			}

			OutValidEdges.Add(Edge);
		}
	}

	int32 FCluster::FindClosestNeighborInDirection(const int32 NodeIndex, const FVector& Direction, const int32 MinNeighborCount) const
	{
		const TArray<FNode>& NodesRef = *Nodes;

		const FNode& Node = NodesRef[NodeIndex];
		int32 Result = -1;
		double LastDot = -1;

		for (const uint64 AdjacencyHash : Node.Adjacency)
		{
			const int32 AdjacentIndex = PCGEx::H64A(AdjacencyHash);
			if (NodesRef[AdjacentIndex].Adjacency.Num() < MinNeighborCount) { continue; }
			if (const double Dot = FVector::DotProduct(Direction, (Node.Position - NodesRef[AdjacentIndex].Position).GetSafeNormal());
				Dot > LastDot)
			{
				LastDot = Dot;
				Result = AdjacentIndex;
			}
		}

		return Result;
	}

	FNode& FCluster::GetOrCreateNodeUnsafe(const TArray<FPCGPoint>& InNodePoints, int32 PointIndex)
	{
		const int32* NodeIndex = NodeIndexLookup->Find(PointIndex);

		if (!NodeIndex)
		{
			NodeIndexLookup->Add(PointIndex, Nodes->Num());
			return Nodes->Emplace_GetRef(Nodes->Num(), PointIndex, InNodePoints[PointIndex].Transform.GetLocation());
		}

		return (*Nodes)[*NodeIndex];
	}

	void FCluster::CreateVtxPointIndices()
	{
		FWriteScopeLock WriteScopeLock(ClusterLock);

		VtxPointIndices = new TArray<int32>();
		VtxPointIndices->SetNum(Nodes->Num());

		const TArray<FNode>& NodesRef = *Nodes;
		TArray<int32>& VtxPointIndicesRef = *VtxPointIndices;
		for (int i = 0; i < VtxPointIndices->Num(); i++) { VtxPointIndicesRef[i] = NodesRef[i].PointIndex; }
	}

	void FCluster::CreateVtxPointScopes()
	{
		if (!VtxPointIndices) { CreateVtxPointIndices(); }

		{
			FWriteScopeLock WriteScopeLock(ClusterLock);
			VtxPointScopes = new TArray<uint64>();
			PCGEx::ScopeIndices(*VtxPointIndices, *VtxPointScopes);
		}
	}

#pragma endregion

#pragma region FNodeProjection

	FNodeProjection::FNodeProjection(FNode* InNode)
		: Node(InNode)
	{
	}

	void FNodeProjection::Project(const FCluster* InCluster, const FPCGExGeo2DProjectionSettings* ProjectionSettings)
	{
		const TArray<FNode>& NodesRef = *InCluster->Nodes;

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
			FVector Direction = ProjectionSettings->Project((Node->Position - NodesRef[PCGEx::H64A(Node->Adjacency[i])].Position), Node->PointIndex);
			Direction.Z = 0;
			Angles[i] = PCGExMath::GetAngle(FVector::ForwardVector, Direction.GetSafeNormal());
		}

		Sort.Sort([&](const int32& A, const int32& B) { return Angles[A] > Angles[B]; });
		for (int i = 0; i < NumNodes; i++) { SortedAdjacency[i] = Node->Adjacency[Sort[i]]; }

		Sort.Empty();
		Angles.Empty();
	}

	void FNodeProjection::ComputeNormal(const FCluster* InCluster)
	{
		const TArray<FNode>& NodesRef = *InCluster->Nodes;

		Normal = FVector::ZeroVector;

		if (SortedAdjacency.IsEmpty())
		{
			Normal = FVector::UpVector;
			return;
		}

		Normal = PCGExMath::GetNormal(NodesRef[PCGEx::H64A(SortedAdjacency.Last())].Position, Node->Position, NodesRef[PCGEx::H64A(SortedAdjacency[0])].Position);

		if (SortedAdjacency.Num() < 2) { return; }

		for (int i = 0; i < SortedAdjacency.Num() - 1; i++)
		{
			Normal += PCGExMath::GetNormal(NodesRef[PCGEx::H64A(SortedAdjacency[i])].Position, Node->Position, NodesRef[PCGEx::H64A(SortedAdjacency[i + 1])].Position);
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
		Nodes.Reserve(Cluster->Nodes->Num());
		for (FNode& Node : (*Cluster->Nodes)) { Nodes.Emplace(&Node); }
	}

	FClusterProjection::~FClusterProjection()
	{
		Nodes.Empty();
	}

	void FClusterProjection::Build()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FClusterProjection::Build);
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
		const TArray<FNode>& NodesRef = *Cluster->Nodes;

		const FNodeProjection& Project = Nodes[NodeIndex];
		const int32 StartIndex = Project.GetAdjacencyIndex(From);
		if (StartIndex == -1) { return -1; }

		const int32 NumNodes = Project.SortedAdjacency.Num();
		for (int i = 0; i < NumNodes; i++)
		{
			const int32 NextIndex = PCGEx::H64A(Project.SortedAdjacency[PCGExMath::Tile(StartIndex + i + 1, 0, NumNodes - 1)]);
			if ((NextIndex == From && NumNodes > 1) ||
				Exclusion.Contains(NextIndex) ||
				NodesRef[NextIndex].Adjacency.Num() < MinNeighbors) { continue; }

			return NextIndex;
		}

		return -1;
	}

	int32 FClusterProjection::FindNextAdjacentNodeCW(const int32 NodeIndex, const int32 From, const TSet<int32>& Exclusion, const int32 MinNeighbors)
	{
		const TArray<FNode>& NodesRef = *Cluster->Nodes;

		const FNodeProjection& Project = Nodes[NodeIndex];
		const int32 StartIndex = Project.GetAdjacencyIndex(From);
		if (StartIndex == -1) { return -1; }

		const int32 NumNodes = Project.SortedAdjacency.Num();
		for (int i = 0; i < NumNodes; i++)
		{
			const int32 NextIndex = PCGEx::H64A(Project.SortedAdjacency[PCGExMath::Tile(StartIndex - i - 1, 0, NumNodes - 1)]);
			if ((NextIndex == From && NumNodes > 1) ||
				Exclusion.Contains(NextIndex) ||
				NodesRef[NextIndex].Adjacency.Num() < MinNeighbors) { continue; }

			return NextIndex;
		}

		return -1;
	}

#pragma endregion

#pragma region FNodeStateHandler

	PCGExDataFilter::EType TClusterNodeFilter::GetFilterType() const { return PCGExDataFilter::EType::ClusterNode; }

	void TClusterNodeFilter::CaptureCluster(const FPCGContext* InContext, const FCluster* InCluster)
	{
		bValid = true;
		CapturedCluster = InCluster;

		Capture(InContext, InCluster->VtxIO);
		if (bValid) { CaptureEdges(InContext, InCluster->EdgesIO); } // Only capture edges if we could capture nodes
	}

	void TClusterNodeFilter::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
	{
		// Do not call Super:: as it sets bValid to true, and we want CaptureCluster to have control
		// This is the filter is invalid if used somewhere that doesn't initialize clusters.
		//TFilterHandler::Capture(PointIO);
	}

	void TClusterNodeFilter::CaptureEdges(const FPCGContext* InContext, const PCGExData::FPointIO* EdgeIO)
	{
	}

	bool TClusterNodeFilter::PrepareForTesting(const PCGExData::FPointIO* PointIO)
	{
		if (bCacheResults)
		{
			const int32 NumNodes = CapturedCluster->Nodes->Num();
			Results.SetNumUninitialized(NumNodes);
			for (int i = 0; i < NumNodes; i++) { Results[i] = false; }
		}

		return false;
	}

	bool TClusterNodeFilter::PrepareForTesting(const PCGExData::FPointIO* PointIO, const TArrayView<const int32>& PointIndices)
	{
		return TFilter::PrepareForTesting(PointIO, PointIndices);
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
				Handler->bCacheResults = false; // So we don't create individual filter caches
				if (Handler->GetFilterType() == PCGExDataFilter::EType::ClusterNode) { ClusterFilterHandlers.Add(static_cast<TClusterNodeFilter*>(Handler)); }
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
		TArray<TClusterNodeFilter*> InvalidClusterTests;

		for (TFilter* Test : FilterHandlers)
		{
			Test->Capture(InContext, Cluster->VtxIO);
			if (!Test->bValid) { InvalidTests.Add(Test); }
		}

		for (TFilter* Filter : InvalidTests)
		{
			FilterHandlers.Remove(Filter);
			delete Filter;
		}

		for (TClusterNodeFilter* Test : ClusterFilterHandlers)
		{
			Test->CaptureCluster(InContext, Cluster);
			if (!Test->bValid) { InvalidClusterTests.Add(Test); }
		}

		for (TClusterNodeFilter* Filter : InvalidClusterTests)
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
			const int32 NodeIndex = (*Cluster->NodeIndexLookup)[PointIndex]; // We get a point index from the FindNode
			for (const TClusterNodeFilter* Test : ClusterFilterHandlers) { if (!Test->Test(NodeIndex)) { return false; } }
		}

		return true;
	}

	bool FNodeStateHandler::PrepareForTesting(const PCGExData::FPointIO* PointIO)
	{
		TDataState::PrepareForTesting(PointIO);

		HeavyFilterHandlers.Empty();
		HeavyClusterFilterHandlers.Empty();

		for (TFilter* Test : FilterHandlers) { if (Test->PrepareForTesting(PointIO)) { HeavyFilterHandlers.Add(Test); } }
		for (TClusterNodeFilter* Test : ClusterFilterHandlers) { if (Test->PrepareForTesting(PointIO)) { HeavyClusterFilterHandlers.Add(Test); } }

		return RequiresPerPointPreparation();
	}

	bool FNodeStateHandler::PrepareForTesting(const PCGExData::FPointIO* PointIO, const TArrayView<const int32>& PointIndices)
	{
		TDataState::PrepareForTesting(PointIO, PointIndices);

		HeavyFilterHandlers.Empty();
		HeavyClusterFilterHandlers.Empty();

		for (TFilter* Test : FilterHandlers) { if (Test->PrepareForTesting(PointIO, PointIndices)) { HeavyFilterHandlers.Add(Test); } }
		for (TClusterNodeFilter* Test : ClusterFilterHandlers) { if (Test->PrepareForTesting(PointIO, PointIndices)) { HeavyClusterFilterHandlers.Add(Test); } }

		return RequiresPerPointPreparation();
	}

	void FNodeStateHandler::PrepareSingle(const int32 PointIndex)
	{
		for (TFilter* Test : HeavyFilterHandlers) { Test->PrepareSingle(PointIndex); }

		if (!HeavyClusterFilterHandlers.IsEmpty())
		{
			const int32 NodeIndex = *Cluster->NodeIndexLookup->Find(PointIndex); // We get a point index from the FindNode
			for (TClusterNodeFilter* Test : HeavyClusterFilterHandlers) { Test->PrepareSingle(NodeIndex); }
		}
	}

	void FNodeStateHandler::PreparationComplete()
	{
		for (TFilter* Test : HeavyFilterHandlers) { Test->PreparationComplete(); }
		for (TClusterNodeFilter* Test : HeavyClusterFilterHandlers) { Test->PreparationComplete(); }
	}

	bool FNodeStateHandler::RequiresPerPointPreparation() const
	{
		return !HeavyFilterHandlers.IsEmpty() || !HeavyClusterFilterHandlers.IsEmpty();
	}

#pragma endregion
}

namespace PCGExClusterTask
{
	bool FBuildCluster::ExecuteTask()
	{
		Cluster->BuildFrom(
			EdgeIO, PointIO->GetIn()->GetPoints(),
			*EndpointsLookup, ExpectedAdjacency);

		return true;
	}

	bool FFindNodeChains::ExecuteTask()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FFindNodeChains::ExecuteTask);

		//TArray<uint64> AdjacencyHashes;
		//AdjacencyHashes.Reserve(Cluster->Nodes.Num());

		TArray<PCGExCluster::FNode>& NodesRefs = *Cluster->Nodes;
		TSet<int32> IgnoredEdges;
		IgnoredEdges.Reserve(Cluster->Nodes->Num());

		bool bIsAlreadyIgnored;


		for (const PCGExCluster::FNode& Node : (*Cluster->Nodes))
		{
			if (Node.IsSimple() && !(*Breakpoints)[Node.NodeIndex]) { continue; }

			const bool bIsValidStartNode =
				bDeadEndsOnly ?
					Node.IsDeadEnd() && !(*Breakpoints)[Node.NodeIndex] :
					(Node.IsDeadEnd() || (*Breakpoints)[Node.NodeIndex] || Node.IsComplex());

			if (!bIsValidStartNode) { continue; }

			for (const uint64 AdjacencyHash : Node.Adjacency)
			{
				uint32 OtherNodeIndex;
				uint32 EdgeIndex;
				PCGEx::H64(AdjacencyHash, OtherNodeIndex, EdgeIndex);

				IgnoredEdges.Add(EdgeIndex, &bIsAlreadyIgnored);
				if (bIsAlreadyIgnored) { continue; }

				const PCGExCluster::FNode& OtherNode = NodesRefs[OtherNodeIndex];

				if ((*Breakpoints)[OtherNode.NodeIndex] || OtherNode.IsDeadEnd() || OtherNode.IsComplex())
				{
					// Single edge chain					
					if (bSkipSingleEdgeChains) { continue; }

					PCGExCluster::FNodeChain* NewChain = new PCGExCluster::FNodeChain();
					NewChain->First = Node.NodeIndex;
					NewChain->Last = OtherNodeIndex;
					NewChain->SingleEdge = EdgeIndex;

					Chains->Add(NewChain);
				}
				else
				{
					// Requires extended Search
					InternalStart<FBuildChain>(
						Chains->Add(nullptr), nullptr,
						Cluster, Breakpoints, Chains, Node.NodeIndex, AdjacencyHash);
				}
			}
		}

		return true;
	}

	bool FBuildChain::ExecuteTask()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBuildChain::ExecuteTask);

		PCGExCluster::FNodeChain* NewChain = new PCGExCluster::FNodeChain();

		uint32 NodeIndex;
		uint32 EdgeIndex;
		PCGEx::H64(AdjacencyHash, NodeIndex, EdgeIndex);

		NewChain->First = StartIndex;
		NewChain->Last = NodeIndex;

		BuildChain(NewChain, Breakpoints, Cluster);
		(*Chains)[TaskIndex] = NewChain;

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
		PCGExData::FPointIO* VtxDupe = VtxCollection->Emplace_GetRef(Vtx->GetIn(), PCGExData::EInit::DuplicateInput);
		VtxDupe->IOIndex = TaskIndex;

		FString OutId;
		PCGExGraph::SetClusterVtx(VtxDupe, OutId);

		InternalStart<PCGExGeoTasks::FTransformPointIO>(TaskIndex, PointIO, VtxDupe, TransformSettings);

		for (const PCGExData::FPointIO* EdgeIO : Edges)
		{
			PCGExData::FPointIO* EdgeDupe = EdgeCollection->Emplace_GetRef(EdgeIO->GetIn(), PCGExData::EInit::DuplicateInput);
			EdgeDupe->IOIndex = TaskIndex;
			PCGExGraph::MarkClusterEdges(EdgeDupe, OutId);

			InternalStart<PCGExGeoTasks::FTransformPointIO>(TaskIndex, PointIO, EdgeDupe, TransformSettings);
		}

		return true;
	}
}
