// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCluster.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Geometry/PCGExGeo.h"
#include "Graph/Data/PCGExClusterData.h"

#pragma region UPCGExNodeStateDefinition

#pragma endregion

namespace PCGExCluster
{
#pragma region FNode

	FNode::~FNode()
	{
		Adjacency.Empty();
	}

	FVector FNode::GetCentroid(const FCluster* InCluster) const
	{
		if (Adjacency.IsEmpty()) { return InCluster->GetPos(NodeIndex); }

		FVector Centroid = FVector::ZeroVector;
		const int32 NumPoints = Adjacency.Num();

		TArray<FNode>& Nodes = *InCluster->Nodes;
		for (int i = 0; i < NumPoints; i++) { Centroid += InCluster->GetPos(PCGEx::H64A(Adjacency[i])); }

		if (Adjacency.Num() < 2)
		{
			Centroid += InCluster->GetPos(NodeIndex);
			return Centroid / 2;
		}

		Centroid /= static_cast<double>(NumPoints);

		return Centroid;
	}

	void FNode::ComputeNormal(const FCluster* InCluster, const TArray<FAdjacencyData>& AdjacencyData, FVector& OutNormal) const
	{
		const int32 NumNeighbors = AdjacencyData.Num();

		OutNormal = FVector::ZeroVector;

		if (AdjacencyData.IsEmpty())
		{
			OutNormal = FVector::UpVector;
			return;
		}

		for (const FAdjacencyData& A : AdjacencyData)
		{
			FVector Position = InCluster->GetPos(NodeIndex);
			OutNormal += PCGExMath::GetNormal(InCluster->GetPos(A.NodeIndex), Position, Position + FVector::ForwardVector);
		}

		OutNormal /= NumNeighbors;
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
	                   const bool bCopyNodes, const bool bCopyEdges, const bool bCopyLookup)
	{
		bIsMirror = true;
		bIsCopyCluster = false;

		VtxIO = InVtxIO;
		EdgesIO = InEdgesIO;

		NumRawVtx = InVtxIO->GetNum();
		NumRawEdges = InEdgesIO->GetNum();

		ExpandedNodes = OtherCluster->ExpandedNodes;
		if (ExpandedNodes) { bOwnsExpandedNodes = false; }

		ExpandedEdges = OtherCluster->ExpandedEdges;
		if (ExpandedEdges) { bOwnsExpandedEdges = false; }

		if (bCopyNodes)
		{
			Nodes = new TArray<FNode>();
			Nodes->Reserve(OtherCluster->Nodes->Num());
			Nodes->Append(*OtherCluster->Nodes);

			ExpandedNodes = nullptr;
			bOwnsExpandedNodes = true;
		}
		else
		{
			bOwnsNodes = false;
			Nodes = OtherCluster->Nodes;
		}

		UpdatePositions();

		if (bCopyEdges)
		{
			Edges = new TArray<PCGExGraph::FIndexedEdge>();
			Edges->Reserve(OtherCluster->Edges->Num());
			Edges->Append(*OtherCluster->Edges);

			ExpandedEdges = nullptr;
			bOwnsExpandedEdges = true;
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

		/*
		EdgeLengths = OtherCluster->EdgeLengths;
		if (EdgeLengths)
		{
			bEdgeLengthsDirty = false;
			bOwnsLengths = false;
		}
		*/

		VtxPointIndices = OtherCluster->VtxPointIndices;
		if (VtxPointIndices) { bOwnsVtxPointIndices = false; }

		NodeOctree = OtherCluster->NodeOctree;
		if (NodeOctree) { bOwnsNodeOctree = false; }

		EdgeOctree = OtherCluster->EdgeOctree;
		if (EdgeOctree) { bOwnsEdgeOctree = false; }
	}

	void FCluster::ClearInheritedForChanges(const bool bClearOwned)
	{
		WillModifyVtxIO(bClearOwned);
		WillModifyVtxPositions(bClearOwned);
	}

	void FCluster::WillModifyVtxIO(const bool bClearOwned)
	{
		if (!bOwnsVtxPointIndices)
		{
			VtxPointIndices = nullptr;
			bOwnsVtxPointIndices = true;
		}
		else if (bClearOwned && VtxPointIndices)
		{
			PCGEX_DELETE(VtxPointIndices)
		}
	}

	void FCluster::WillModifyVtxPositions(const bool bClearOwned)
	{
		if (!bOwnsLengths)
		{
			EdgeLengths = nullptr;
			bOwnsLengths = true;
		}
		else if (bClearOwned && EdgeLengths)
		{
			PCGEX_DELETE(EdgeLengths)
		}

		if (!bOwnsNodeOctree)
		{
			NodeOctree = nullptr;
			bOwnsNodeOctree = true;
		}
		else if (bClearOwned && NodeOctree)
		{
			PCGEX_DELETE(NodeOctree)
		}

		if (!bOwnsEdgeOctree)
		{
			EdgeOctree = nullptr;
			bOwnsEdgeOctree = true;
		}
		else if (bClearOwned && EdgeOctree)
		{
			PCGEX_DELETE(EdgeOctree)
		}

		if (!bOwnsExpandedNodes)
		{
			ExpandedNodes = nullptr;
			bOwnsExpandedNodes = true;
		}
		else if (bClearOwned && ExpandedNodes)
		{
			PCGEX_DELETE_TARRAY_FULL(ExpandedNodes)
		}

		if (!bOwnsExpandedEdges)
		{
			ExpandedEdges = nullptr;
			bOwnsExpandedEdges = true;
		}
		else if (bClearOwned && ExpandedEdges)
		{
			PCGEX_DELETE_TARRAY_FULL(ExpandedEdges)
		}
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
		if (bOwnsExpandedNodes) { PCGEX_DELETE(ExpandedNodes) }
		if (bOwnsExpandedEdges) { PCGEX_DELETE(ExpandedEdges) }

		NodePositions.Empty();
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

		Bounds = Bounds.ExpandBy(10);

		return true;
	}

	void FCluster::BuildFrom(const PCGExGraph::FSubGraph* SubGraph)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCluster::BuildClusterFromSubgraph);

		Bounds = FBox(ForceInit);

		NumRawVtx = SubGraph->VtxIO->GetNum(PCGExData::ESource::Out);
		NumRawEdges = SubGraph->EdgesIO->GetNum(PCGExData::ESource::Out);

		const TArray<FPCGPoint>& VtxPoints = SubGraph->VtxIO->GetOutIn()->GetPoints();
		Nodes->Reserve(SubGraph->Nodes.Num());

		Edges->Reserve(NumRawEdges);
		Edges->Append(SubGraph->FlattenedEdges);

		// TODO : This can probably be more optimal

		for (const PCGExGraph::FIndexedEdge& E : SubGraph->FlattenedEdges)
		{
			FNode& StartNode = GetOrCreateNodeUnsafe(VtxPoints, E.Start);
			FNode& EndNode = GetOrCreateNodeUnsafe(VtxPoints, E.End);

			StartNode.Add(EndNode, E.EdgeIndex);
			EndNode.Add(StartNode, E.EdgeIndex);
		}

		Bounds = Bounds.ExpandBy(10);
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

	void FCluster::RebuildNodeOctree()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCluster::RebuildNodeOctree);

		PCGEX_DELETE(NodeOctree)
		const FPCGPoint* StartPtr = VtxIO->GetIn()->GetPoints().GetData();
		NodeOctree = new ClusterItemOctree(Bounds.GetCenter(), (Bounds.GetExtent() + FVector(10)).Length());
		for (int i = 0; i < Nodes->Num(); i++)
		{
			const FNode* Node = Nodes->GetData() + i;
			const FPCGPoint* Pt = StartPtr + Node->PointIndex;
			NodeOctree->AddElement(FClusterItemRef(Node->NodeIndex, FBoxSphereBounds(Pt->GetLocalBounds().TransformBy(Pt->Transform))));
		}
	}

	void FCluster::RebuildEdgeOctree()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCluster::RebuildEdgeOctree);

		check(Bounds.GetExtent().Length() != 0)

		PCGEX_DELETE(EdgeOctree)

		EdgeOctree = new ClusterItemOctree(Bounds.GetCenter(), (Bounds.GetExtent() + FVector(10)).Length());

		if (!ExpandedEdges)
		{
			bOwnsExpandedEdges = true;
			ExpandedEdges = new TArray<FExpandedEdge*>();
			PCGEX_SET_NUM_UNINITIALIZED_PTR(ExpandedEdges, Edges->Num());

			TArray<FExpandedEdge*>& ExpandedEdgesRef = (*ExpandedEdges);

			for (int i = 0; i < Edges->Num(); i++)
			{
				FExpandedEdge* NewExpandedEdge = new FExpandedEdge(this, i);
				ExpandedEdgesRef[i] = NewExpandedEdge;
				EdgeOctree->AddElement(FClusterItemRef(i, NewExpandedEdge->Bounds));
			}
		}
		else
		{
			for (int i = 0; i < Edges->Num(); i++)
			{
				const FExpandedEdge* ExpandedEdge = *(ExpandedEdges->GetData() + i);
				EdgeOctree->AddElement(
						FClusterItemRef(i, ExpandedEdge->Bounds)
					);
			}
		}
	}

	void FCluster::RebuildOctree(const EPCGExClusterClosestSearchMode Mode, const bool bForceRebuild)
	{
		switch (Mode)
		{
		case EPCGExClusterClosestSearchMode::Node:
			if (NodeOctree && !bForceRebuild) { return; }
			RebuildNodeOctree();
			break;
		case EPCGExClusterClosestSearchMode::Edge:
			if (EdgeOctree && !bForceRebuild) { return; }
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
				const double Dist = FVector::DistSquared(Position, GetPos(Node));
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
				const double Dist = FVector::DistSquared(Position, GetPos(Node));
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
				const FExpandedEdge* Edge = *(ExpandedEdges->GetData() + Item.ItemIndex);
				const double Dist = FMath::PointDistToSegmentSquared(Position, GetPos(Edge->Start), GetPos(Edge->End));
				if (Dist < MaxDistance)
				{
					MaxDistance = Dist;
					ClosestIndex = Edge->Index;
				}
			};

			EdgeOctree->FindNearbyElements(Position, ProcessCandidate);
		}
		else if (ExpandedEdges)
		{
			for (const FExpandedEdge* Edge : (*ExpandedEdges))
			{
				const double Dist = FMath::PointDistToSegmentSquared(Position, GetPos(Edge->Start), GetPos(Edge->End));
				if (Dist < MaxDistance)
				{
					MaxDistance = Dist;
					ClosestIndex = Edge->Index;
				}
			}
		}
		else
		{
			for (const PCGExGraph::FIndexedEdge& Edge : (*Edges))
			{
				const FNode& Start = NodesRef[NodeIndexLookupRef[Edge.Start]];
				const FNode& End = NodesRef[NodeIndexLookupRef[Edge.End]];
				const double Dist = FMath::PointDistToSegmentSquared(Position, GetPos(Start), GetPos(End));
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

		ClosestIndex = FVector::DistSquared(Position, GetPos(Start)) < FVector::DistSquared(Position, GetPos(End)) ? Start.NodeIndex : End.NodeIndex;

		return ClosestIndex;
	}

	int32 FCluster::FindClosestEdge(const int32 InNodeIndex, const FVector& InPosition) const
	{
		if (!Nodes->IsValidIndex(InNodeIndex) || (Nodes->GetData() + InNodeIndex)->Adjacency.IsEmpty()) { return -1; }
		const FNode& Node = *(Nodes->GetData() + InNodeIndex);

		double MinDist = TNumericLimits<double>::Max();
		int32 BestIndex = -1;

		double BestDot = 1;
		FVector Position = GetPos(Node);
		const FVector SearchDirection = (GetPos(Node) - InPosition).GetSafeNormal();

		if (ExpandedNodes)
		{
			const FExpandedNode* ENode = *(ExpandedNodes->GetData() + Node.NodeIndex);

			for (const FExpandedNeighbor& N : ENode->Neighbors)
			{
				const double Dist = FMath::PointDistToSegmentSquared(InPosition, Position, GetPos(N.Node));
				if (Dist < MinDist)
				{
					MinDist = Dist;
					BestIndex = N.Edge->EdgeIndex;
				}
				else if (Dist == MinDist)
				{
					if (const double Dot = FVector::DotProduct(SearchDirection, (GetPos(N.Node) - Position).GetSafeNormal());
						Dot < BestDot)
					{
						BestDot = Dot;
						BestIndex = N.Edge->EdgeIndex;
					}
				}
			}
		}
		else
		{
			for (const int64 H : Node.Adjacency)
			{
				uint32 OtherNodeIndex;
				uint32 OtherEdgeIndex;
				PCGEx::H64(H, OtherNodeIndex, OtherEdgeIndex);
				FVector NPos = GetPos(OtherNodeIndex);
				const double Dist = FMath::PointDistToSegmentSquared(InPosition, Position, NPos);
				if (Dist < MinDist)
				{
					MinDist = Dist;
					BestIndex = OtherEdgeIndex;
				}
				else if (Dist == MinDist)
				{
					if (const double Dot = FVector::DotProduct(SearchDirection, (NPos - Position).GetSafeNormal());
						Dot < BestDot)
					{
						BestDot = Dot;
						BestIndex = OtherEdgeIndex;
					}
				}
			}
		}

		return BestIndex;
	}

	int32 FCluster::FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, const int32 MinNeighborCount) const
	{
		const TArray<FNode>& NodesRef = *Nodes;
		const FNode& Node = NodesRef[NodeIndex];
		int32 Result = -1;
		double LastDist = TNumericLimits<double>::Max();
		const FVector NodePosition = GetPos(NodeIndex);

		if (NodeOctree)
		{
			auto ProcessCandidate = [&](const FClusterItemRef& Item)
			{
				if (NodesRef[Item.ItemIndex].Adjacency.Num() < MinNeighborCount) { return; }
				if (const double Dist = FMath::PointDistToSegmentSquared(Position, NodePosition, GetPos(Item.ItemIndex));
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
				if (const double Dist = FMath::PointDistToSegmentSquared(Position, NodePosition, GetPos(OtherIndex));
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
		const FVector NodePosition = GetPos(NodeIndex);

		if (NodeOctree)
		{
			auto ProcessCandidate = [&](const FClusterItemRef& Item)
			{
				if (NodesRef[Item.ItemIndex].Adjacency.Num() < MinNeighborCount) { return; }
				if (Exclusion.Contains(Item.ItemIndex)) { return; }
				if (const double Dist = FMath::PointDistToSegmentSquared(Position, NodePosition, GetPos(Item.ItemIndex));
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
				if (const double Dist = FMath::PointDistToSegmentSquared(Position, NodePosition, GetPos(OtherIndex));
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
			const double Dist = GetDistSquared(NodeIndexLookupRef[Edge.Start], NodeIndexLookupRef[Edge.End]);
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

	void FCluster::GetValidEdges(TArray<PCGExGraph::FIndexedEdge>& OutValidEdges) const
	{
		TMap<int32, int32>& LookupRef = (*NodeIndexLookup);
		for (const PCGExGraph::FIndexedEdge& Edge : (*Edges))
		{
			if (!Edge.bValid ||
				!(Nodes->GetData() + LookupRef[Edge.Start])->bValid || // Adds quite the cost
				!(Nodes->GetData() + LookupRef[Edge.End])->bValid)
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
		FVector Position = GetPos(NodeIndex);

		for (const uint64 AdjacencyHash : Node.Adjacency)
		{
			const int32 AdjacentIndex = PCGEx::H64A(AdjacencyHash);
			if (NodesRef[AdjacentIndex].Adjacency.Num() < MinNeighborCount) { continue; }
			if (const double Dot = FVector::DotProduct(Direction, GetDir(NodeIndex, AdjacentIndex));
				Dot > LastDot)
			{
				LastDot = Dot;
				Result = AdjacentIndex;
			}
		}

		return Result;
	}

	TArray<FExpandedNode*>* FCluster::GetExpandedNodes(const bool bBuild)
	{
		{
			FReadScopeLock ReadScopeLock(ClusterLock);
			if (ExpandedNodes) { return ExpandedNodes; }
		}
		{
			FWriteScopeLock WriteScopeLock(ClusterLock);

			bOwnsExpandedNodes = true;
			ExpandedNodes = new TArray<FExpandedNode*>();
			PCGEX_SET_NUM_UNINITIALIZED_PTR(ExpandedNodes, Nodes->Num())

			TArray<FExpandedNode*>& ExpandedNodesRef = (*ExpandedNodes);
			if (bBuild) { for (int i = 0; i < ExpandedNodes->Num(); i++) { ExpandedNodesRef[i] = new FExpandedNode(this, i); } } // Ooof
		}

		return ExpandedNodes;
	}

	void FCluster::ExpandNodes(PCGExMT::FTaskManager* AsyncManager)
	{
		if (ExpandedNodes) { return; }

		bOwnsExpandedNodes = true;
		ExpandedNodes = new TArray<FExpandedNode*>();
		PCGEX_SET_NUM_UNINITIALIZED_PTR(ExpandedNodes, Nodes->Num())

		PCGExMT::SubRanges(
			Nodes->Num(), 256, [&](const int32 Start, const int32 Count)
			{
				AsyncManager->Start<PCGExClusterTask::FExpandClusterNodes>(Start, nullptr, this, Count);
			});
	}

	TArray<FExpandedEdge*>* FCluster::GetExpandedEdges(const bool bBuild)
	{
		{
			FReadScopeLock ReadScopeLock(ClusterLock);
			if (ExpandedEdges) { return ExpandedEdges; }
		}
		{
			FWriteScopeLock WriteScopeLock(ClusterLock);

			bOwnsExpandedEdges = true;
			ExpandedEdges = new TArray<FExpandedEdge*>();
			PCGEX_SET_NUM_UNINITIALIZED_PTR(ExpandedEdges, Edges->Num())

			TArray<FExpandedEdge*>& ExpandedEdgesRef = (*ExpandedEdges);
			if (bBuild) { for (int i = 0; i < ExpandedEdges->Num(); i++) { ExpandedEdgesRef[i] = new FExpandedEdge(this, i); } } // Ooof
		}

		return ExpandedEdges;
	}

	void FCluster::ExpandEdges(PCGExMT::FTaskManager* AsyncManager)
	{
		if (ExpandedEdges) { return; }

		bOwnsExpandedEdges = true;
		ExpandedEdges = new TArray<FExpandedEdge*>();
		PCGEX_SET_NUM_UNINITIALIZED_PTR(ExpandedEdges, Edges->Num())

		PCGExMT::SubRanges(
			Edges->Num(), 256, [&](const int32 Start, const int32 Count)
			{
				AsyncManager->Start<PCGExClusterTask::FExpandClusterEdges>(Start, nullptr, this, Count);
			});
	}

	void FCluster::UpdatePositions()
	{
		check(VtxIO)

		const TArray<FPCGPoint>& VtxPoints = VtxIO->GetIn()->GetPoints();
		PCGEX_SET_NUM_UNINITIALIZED(NodePositions, Nodes->Num())
		for (const FNode& N : *Nodes) { NodePositions[N.NodeIndex] = VtxPoints[N.PointIndex].Transform.GetLocation(); }
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
			if (Node.IsSimple() && !*(Breakpoints->GetData() + Node.NodeIndex)) { continue; }

			const bool bIsValidStartNode =
				bDeadEndsOnly ?
					Node.IsDeadEnd() && !*(Breakpoints->GetData() + Node.NodeIndex) :
					(Node.IsDeadEnd() || *(Breakpoints->GetData() + Node.NodeIndex) || Node.IsComplex());

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

	bool FExpandClusterNodes::ExecuteTask()
	{
		TArray<PCGExCluster::FExpandedNode*>& ExpandedNodesRef = (*Cluster->ExpandedNodes);
		for (int i = 0; i < NumIterations; i++) { ExpandedNodesRef[TaskIndex + i] = new PCGExCluster::FExpandedNode(Cluster, TaskIndex + i); }
		return true;
	}

	bool FExpandClusterEdges::ExecuteTask()
	{
		TArray<PCGExCluster::FExpandedEdge*>& ExpandedEdgesRef = (*Cluster->ExpandedEdges);
		for (int i = 0; i < NumIterations; i++) { ExpandedEdgesRef[TaskIndex + i] = new PCGExCluster::FExpandedEdge(Cluster, TaskIndex + i); }
		return true;
	}
}
