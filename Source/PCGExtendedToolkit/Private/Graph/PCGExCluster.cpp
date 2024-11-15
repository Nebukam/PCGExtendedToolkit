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

	int32 FNode::ValidEdges(const FCluster* InCluster)
	{
		int32 ValidNum = 0;
		for (const uint64 H : Adjacency) { if (InCluster->GetEdge(PCGEx::H64B(H))->bValid) { ValidNum++; } }
		return ValidNum;
	}

	bool FNode::HasAnyValidEdges(const FCluster* InCluster)
	{
		for (const uint64 H : Adjacency) { if (InCluster->GetEdge(PCGEx::H64B(H))->bValid) { return true; } }
		return false;
	}

#pragma endregion

#pragma region FCluster

	FCluster::FCluster(const TSharedPtr<PCGExData::FPointIO>& InVtxIO, const TSharedPtr<PCGExData::FPointIO>& InEdgesIO,
	                   const TSharedPtr<PCGEx::FIndexLookup>& InNodeIndexLookup):
		NodeIndexLookup(InNodeIndexLookup), VtxIO(InVtxIO), EdgesIO(InEdgesIO)
	{
		Nodes = MakeShared<TArray<FNode>>();
		Edges = MakeShared<TArray<PCGExGraph::FIndexedEdge>>();
		Bounds = FBox(ForceInit);

		VtxPoints = &InVtxIO->GetPoints(PCGExData::ESource::In);
	}

	FCluster::FCluster(const TSharedRef<FCluster>& OtherCluster,
	                   const TSharedPtr<PCGExData::FPointIO>& InVtxIO,
	                   const TSharedPtr<PCGExData::FPointIO>& InEdgesIO,
	                   const TSharedPtr<PCGEx::FIndexLookup>& InNodeIndexLookup,
	                   const bool bCopyNodes, const bool bCopyEdges, const bool bCopyLookup):
		NodeIndexLookup(InNodeIndexLookup), VtxIO(InVtxIO), EdgesIO(InEdgesIO)
	{
		NodeIndexLookup = InNodeIndexLookup;
		VtxPoints = &InVtxIO->GetPoints(PCGExData::ESource::In);

		bIsMirror = true;
		bIsCopyCluster = false;

		NumRawVtx = InVtxIO->GetNum();
		NumRawEdges = InEdgesIO->GetNum();

		Bounds = OtherCluster->Bounds;

		ExpandedNodes = OtherCluster->ExpandedNodes;
		ExpandedEdges = OtherCluster->ExpandedEdges;

		if (bCopyNodes)
		{
			const int32 NumNewNodes = OtherCluster->Nodes->Num();

			Nodes = MakeShared<TArray<FNode>>();
			TArray<FNode>& NewNodes = *Nodes;
			TArray<FNode>& SourceNodes = *OtherCluster->Nodes;

			PCGEx::InitArray(NewNodes, NumNewNodes);

			for (int i = 0; i < NumNewNodes; i++)
			{
				const FNode& NewNode = (NewNodes[i] = SourceNodes[i]);

				// Update index lookup
				NodeIndexLookup->GetMutable(NewNode.PointIndex) = NewNode.NodeIndex;
			}

			ExpandedNodes.Reset();
		}
		else
		{
			Nodes = OtherCluster->Nodes;

			// Update index lookup
			for (const FNode& Node : *Nodes) { NodeIndexLookup->GetMutable(Node.PointIndex) = Node.NodeIndex; }
		}

		UpdatePositions();

		if (bCopyEdges)
		{
			const int32 NumNewEdges = OtherCluster->Edges->Num();

			Edges = MakeShared<TArray<PCGExGraph::FIndexedEdge>>();
			TArray<PCGExGraph::FIndexedEdge>& NewEdges = *Edges;
			TArray<PCGExGraph::FIndexedEdge>& SourceEdges = *OtherCluster->Edges;

			PCGEx::InitArray(NewEdges, NumNewEdges);

			const int32 EdgeIOIndex = InEdgesIO->IOIndex;
			for (int i = 0; i < NumNewEdges; i++)
			{
				const PCGExGraph::FIndexedEdge& SourceEdge = SourceEdges[i];
				PCGExGraph::FIndexedEdge& NewEdge = (NewEdges[i] = SourceEdge);
				NewEdge.IOIndex = EdgeIOIndex;
			}

			ExpandedEdges.Reset();
		}
		else
		{
			Edges = OtherCluster->Edges;
		}

		NodeOctree = OtherCluster->NodeOctree;
		EdgeOctree = OtherCluster->EdgeOctree;
	}

	void FCluster::ClearInheritedForChanges(const bool bClearOwned)
	{
		WillModifyVtxIO(bClearOwned);
		WillModifyVtxPositions(bClearOwned);
	}

	void FCluster::WillModifyVtxIO(const bool bClearOwned)
	{
	}

	void FCluster::WillModifyVtxPositions(const bool bClearOwned)
	{
		NodeOctree.Reset();
		EdgeOctree.Reset();
		ExpandedNodes.Reset();
		ExpandedEdges.Reset();
	}

	FCluster::~FCluster()
	{
		NodePositions.Empty();
	}

	bool FCluster::BuildFrom(
		const TMap<uint32, int32>& InEndpointsLookup,
		const TArray<int32>* InExpectedAdjacency,
		const PCGExData::ESource PointsSource)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCluster::BuildCluster);

		const TSharedPtr<PCGExData::FPointIO> PinnedVtxIO = VtxIO.Pin();
		const TSharedPtr<PCGExData::FPointIO> PinnedEdgesIO = EdgesIO.Pin();
		const int32 EdgeIOIndex = PinnedEdgesIO ? PinnedEdgesIO->IOIndex : -1;

		if (!PinnedVtxIO || !PinnedEdgesIO) { return false; }

		const TArray<FPCGPoint>& InNodePoints = PinnedVtxIO->GetPoints(PointsSource);

		Nodes->Empty();
		Edges->Empty();

		const TUniquePtr<PCGExData::TBuffer<int64>> EndpointsBuffer = MakeUnique<PCGExData::TBuffer<int64>>(PinnedEdgesIO.ToSharedRef(), PCGExGraph::Tag_EdgeEndpoints);
		if (!EndpointsBuffer->PrepareRead()) { return false; }

		NumRawVtx = InNodePoints.Num();
		NumRawEdges = PinnedEdgesIO->GetNum();

		auto OnFail = [&]()
		{
			Nodes->Empty();
			Edges->Empty();
			return false;
		};

		const int32 NumEdges = PinnedEdgesIO->GetNum();

		PCGEx::InitArray(Edges, NumEdges);
		Nodes->Reserve(InNodePoints.Num());

		const TArray<int64>& Endpoints = *EndpointsBuffer->GetInValues().Get();

		for (int i = 0; i < NumEdges; i++)
		{
			uint32 A;
			uint32 B;
			PCGEx::H64(Endpoints[i], A, B);

			const int32* StartPointIndexPtr = InEndpointsLookup.Find(A);
			const int32* EndPointIndexPtr = InEndpointsLookup.Find(B);

			if ((!StartPointIndexPtr || !EndPointIndexPtr)) { return OnFail(); }

			FNode& StartNode = GetOrCreateNodeUnsafe(InNodePoints, *StartPointIndexPtr);
			FNode& EndNode = GetOrCreateNodeUnsafe(InNodePoints, *EndPointIndexPtr);

			StartNode.Add(EndNode, i);
			EndNode.Add(StartNode, i);

			*(Edges->GetData() + i) = PCGExGraph::FIndexedEdge(i, *StartPointIndexPtr, *EndPointIndexPtr, i, EdgeIOIndex);
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

		Nodes->Shrink();
		Bounds = Bounds.ExpandBy(10);

		return true;
	}

	void FCluster::BuildFrom(const PCGExGraph::FSubGraph* SubGraph)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCluster::BuildClusterFromSubgraph);

		Bounds = FBox(ForceInit);

		NumRawVtx = SubGraph->VtxDataFacade->Source->GetNum(PCGExData::ESource::Out);
		NumRawEdges = SubGraph->EdgesDataFacade->Source->GetNum(PCGExData::ESource::Out);

		const TArray<FPCGPoint>& SubVtxPoints = SubGraph->VtxDataFacade->Source->GetOutIn()->GetPoints();
		Nodes->Reserve(SubGraph->Nodes.Num());

		Edges->Reserve(NumRawEdges);
		Edges->Append(SubGraph->FlattenedEdges);

		for (const PCGExGraph::FIndexedEdge& E : SubGraph->FlattenedEdges)
		{
			FNode& StartNode = GetOrCreateNodeUnsafe(SubVtxPoints, E.Start);
			FNode& EndNode = GetOrCreateNodeUnsafe(SubVtxPoints, E.End);

			StartNode.Add(EndNode, E.EdgeIndex);
			EndNode.Add(StartNode, E.EdgeIndex);
		}

		Bounds = Bounds.ExpandBy(10);
	}

	bool FCluster::IsValidWith(const TSharedRef<PCGExData::FPointIO>& InVtxIO, const TSharedRef<PCGExData::FPointIO>& InEdgesIO) const
	{
		return NumRawVtx == InVtxIO->GetNum() && NumRawEdges == InEdgesIO->GetNum();
	}

	void FCluster::RebuildNodeOctree()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCluster::RebuildNodeOctree);

		const FPCGPoint* StartPtr = VtxPoints->GetData();
		NodeOctree = MakeShared<PCGEx::FIndexedItemOctree>(Bounds.GetCenter(), (Bounds.GetExtent() + FVector(10)).Length());
		for (int i = 0; i < Nodes->Num(); i++)
		{
			const FNode* Node = Nodes->GetData() + i;
			const FPCGPoint* Pt = StartPtr + Node->PointIndex;
			NodeOctree->AddElement(PCGEx::FIndexedItem(Node->NodeIndex, FBoxSphereBounds(Pt->GetLocalBounds().TransformBy(Pt->Transform))));
		}
	}

	void FCluster::RebuildEdgeOctree()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCluster::RebuildEdgeOctree);

		check(Bounds.GetExtent().Length() != 0)

		EdgeOctree = MakeShared<PCGEx::FIndexedItemOctree>(Bounds.GetCenter(), (Bounds.GetExtent() + FVector(10)).Length());

		if (!ExpandedEdges)
		{
			ExpandedEdges = MakeShared<TArray<FExpandedEdge>>();
			PCGEx::InitArray(ExpandedEdges, Edges->Num());

			TArray<FExpandedEdge>& ExpandedEdgesRef = (*ExpandedEdges);

			for (int i = 0; i < Edges->Num(); i++)
			{
				const FExpandedEdge& NewExpandedEdge = (ExpandedEdgesRef[i] = FExpandedEdge(this, i));
				EdgeOctree->AddElement(PCGEx::FIndexedItem(i, NewExpandedEdge.BSB));
			}
		}
		else
		{
			for (int i = 0; i < Edges->Num(); i++)
			{
				const FExpandedEdge& ExpandedEdge = *(ExpandedEdges->GetData() + i);
				EdgeOctree->AddElement(PCGEx::FIndexedItem(i, ExpandedEdge.BSB));
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
		double MaxDistance = MAX_dbl;
		int32 ClosestIndex = -1;

		const TArray<FNode>& NodesRef = *Nodes;

		if (NodeOctree)
		{
			auto ProcessCandidate = [&](const PCGEx::FIndexedItem& Item)
			{
				const FNode& Node = NodesRef[Item.Index];
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
		double MaxDistance = MAX_dbl;
		int32 ClosestIndex = -1;

		const TArray<FNode>& NodesRef = *Nodes;
		const TArray<PCGExGraph::FIndexedEdge>& EdgesRef = *Edges;

		if (EdgeOctree)
		{
			auto ProcessCandidate = [&](const PCGEx::FIndexedItem& Item)
			{
				const double Dist = GetPointDistToEdgeSquared(Item.Index, Position);
				if (Dist < MaxDistance)
				{
					MaxDistance = Dist;
					ClosestIndex = Item.Index;
				}
			};

			EdgeOctree->FindNearbyElements(Position, ProcessCandidate);
		}
		else if (ExpandedEdges)
		{
			for (const FExpandedEdge& Edge : (*ExpandedEdges))
			{
				const double Dist = GetPointDistToEdgeSquared(Edge.Index, Position);
				if (Dist < MaxDistance)
				{
					MaxDistance = Dist;
					ClosestIndex = Edge.Index;
				}
			}
		}
		else
		{
			for (const PCGExGraph::FIndexedEdge& Edge : (*Edges))
			{
				const double Dist = GetPointDistToEdgeSquared(Edge, Position);
				if (Dist < MaxDistance)
				{
					MaxDistance = Dist;
					ClosestIndex = Edge.EdgeIndex;
				}
			}
		}

		if (ClosestIndex == -1) { return -1; }

		const PCGExGraph::FIndexedEdge& Edge = EdgesRef[ClosestIndex];
		const FNode* Start = GetEdgeStart(Edge);
		const FNode* End = GetEdgeEnd(Edge);

		ClosestIndex = FVector::DistSquared(Position, GetPos(Start)) < FVector::DistSquared(Position, GetPos(End)) ? Start->NodeIndex : End->NodeIndex;

		return ClosestIndex;
	}

	int32 FCluster::FindClosestEdge(const int32 InNodeIndex, const FVector& InPosition) const
	{
		if (!Nodes->IsValidIndex(InNodeIndex) || (Nodes->GetData() + InNodeIndex)->Adjacency.IsEmpty()) { return -1; }
		const FNode& Node = *(Nodes->GetData() + InNodeIndex);

		double MinDist = MAX_dbl;
		int32 BestIndex = -1;

		double BestDot = 1;
		const FVector Position = GetPos(Node);
		const FVector SearchDirection = (GetPos(Node) - InPosition).GetSafeNormal();

		if (ExpandedNodes)
		{
			const FExpandedNode& ENode = *(ExpandedNodes->GetData() + Node.NodeIndex);

			for (const FExpandedNeighbor& N : ENode.Neighbors)
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
		double LastDist = MAX_dbl;
		const FVector NodePosition = GetPos(NodeIndex);

		if (NodeOctree)
		{
			auto ProcessCandidate = [&](const PCGEx::FIndexedItem& Item)
			{
				if (NodesRef[Item.Index].Adjacency.Num() < MinNeighborCount) { return; }
				if (const double Dist = FMath::PointDistToSegmentSquared(Position, NodePosition, GetPos(Item.Index));
					Dist < LastDist)
				{
					LastDist = Dist;
					Result = Item.Index;
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
		double LastDist = MAX_dbl;
		const FVector NodePosition = GetPos(NodeIndex);

		if (NodeOctree)
		{
			auto ProcessCandidate = [&](const PCGEx::FIndexedItem& Item)
			{
				if (NodesRef[Item.Index].Adjacency.Num() < MinNeighborCount) { return; }
				if (Exclusion.Contains(Item.Index)) { return; }
				if (const double Dist = FMath::PointDistToSegmentSquared(Position, NodePosition, GetPos(Item.Index));
					Dist < LastDist)
				{
					LastDist = Dist;
					Result = Item.Index;
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

		EdgeLengths = MakeShared<TArray<double>>();
		TArray<double>& LengthsRef = *EdgeLengths;

		const TArray<FNode>& NodesRef = *Nodes;
		const TArray<PCGExGraph::FIndexedEdge>& EdgesRef = *Edges;

		const int32 NumEdges = Edges->Num();
		double Min = MAX_dbl;
		double Max = MIN_dbl;
		EdgeLengths->SetNumUninitialized(NumEdges);

		for (int i = 0; i < NumEdges; i++)
		{
			const PCGExGraph::FIndexedEdge& Edge = EdgesRef[i];
			const double Dist = GetDist(Edge);
			LengthsRef[i] = Dist;
			Min = FMath::Min(Dist, Min);
			Max = FMath::Max(Dist, Max);
		}

		if (bNormalize) { for (int i = 0; i < NumEdges; i++) { LengthsRef[i] /= Max; } }

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
		const TSharedPtr<PCGExData::FPointIO> PinnedEdgesIO = EdgesIO.Pin();
		OutValidEdges.Reserve(Edges->Num());

		const int32 IOIndex = PinnedEdgesIO ? PinnedEdgesIO->IOIndex : -1;

		for (const PCGExGraph::FIndexedEdge& Edge : (*Edges))
		{
			if (!Edge.bValid || !GetEdgeStart(Edge)->bValid || !GetEdgeEnd(Edge)->bValid) { continue; }
			OutValidEdges.Add_GetRef(Edge).IOIndex = IOIndex;
		}

		OutValidEdges.Shrink();
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

	TSharedPtr<TArray<FExpandedNode>> FCluster::GetExpandedNodes(const bool bBuild)
	{
		{
			FReadScopeLock ReadScopeLock(ClusterLock);
			if (ExpandedNodes) { return ExpandedNodes; }
		}
		{
			FWriteScopeLock WriteScopeLock(ClusterLock);

			ExpandedNodes = MakeShared<TArray<FExpandedNode>>();
			PCGEx::InitArray(ExpandedNodes, Nodes->Num());

			TArray<FExpandedNode>& ExpandedNodesRef = (*ExpandedNodes);
			const TSharedPtr<FCluster> SharedPtr = SharedThis(this);
			if (bBuild) { for (int i = 0; i < ExpandedNodes->Num(); i++) { ExpandedNodesRef[i] = FExpandedNode(SharedPtr, i); } } // Ooof
		}

		return ExpandedNodes;
	}

	void FCluster::ExpandNodes(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		if (ExpandedNodes) { return; }

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ExpandNodesTask);

		ExpandedNodes = MakeShared<TArray<FExpandedNode>>();
		PCGEx::InitArray(ExpandedNodes, Nodes->Num());

		ExpandNodesTask->OnSubLoopStartCallback =
			[WeakThis = TWeakPtr<FCluster>(SharedThis(this))]
			(const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				const TSharedPtr<FCluster> This = WeakThis.Pin();
				if (!This) { return; }

				TArray<FExpandedNode>& ExpandedNodesRef = (*This->ExpandedNodes);
				const int32 MaxIndex = StartIndex + Count;
				for (int i = StartIndex; i < MaxIndex; i++) { ExpandedNodesRef[i] = FExpandedNode(This, i); }
			};

		ExpandNodesTask->StartSubLoops(Nodes->Num(), 256);
	}

	TSharedPtr<TArray<FExpandedEdge>> FCluster::GetExpandedEdges(const bool bBuild)
	{
		{
			FReadScopeLock ReadScopeLock(ClusterLock);
			if (ExpandedEdges) { return ExpandedEdges; }
		}
		{
			FWriteScopeLock WriteScopeLock(ClusterLock);

			ExpandedEdges = MakeShared<TArray<FExpandedEdge>>();
			PCGEx::InitArray(ExpandedEdges, Edges->Num());

			TArray<FExpandedEdge>& ExpandedEdgesRef = (*ExpandedEdges);
			if (bBuild) { for (int i = 0; i < ExpandedEdges->Num(); i++) { ExpandedEdgesRef[i] = FExpandedEdge(this, i); } } // Ooof
		}

		return ExpandedEdges;
	}

	void FCluster::ExpandEdges(PCGExMT::FTaskManager* AsyncManager)
	{
		if (ExpandedEdges) { return; }

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ExpandEdgesTask);

		ExpandedEdges = MakeShared<TArray<FExpandedEdge>>();
		PCGEx::InitArray(ExpandedEdges, Edges->Num());

		ExpandEdgesTask->OnSubLoopStartCallback =
			[WeakThis = TWeakPtr<FCluster>(SharedThis(this))]
			(const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				const TSharedPtr<FCluster> This = WeakThis.Pin();
				if (!This) { return; }

				TArray<FExpandedEdge>& ExpandedEdgesRef = (*This->ExpandedEdges);
				const FCluster* Cluster = This.Get();
				const int32 MaxIndex = StartIndex + Count;
				for (int i = StartIndex; i < MaxIndex; i++) { ExpandedEdgesRef[i] = FExpandedEdge(Cluster, i); }
			};

		ExpandEdgesTask->StartSubLoops(Edges->Num(), 256);
	}

	void FCluster::UpdatePositions()
	{
		const TArray<FPCGPoint>& VtxPointsRef = *VtxPoints;
		NodePositions.SetNumUninitialized(Nodes->Num());
		Bounds = FBox(ForceInit);
		for (const FNode& N : *Nodes)
		{
			const FVector Pos = VtxPointsRef[N.PointIndex].Transform.GetLocation();
			NodePositions[N.NodeIndex] = Pos;
			Bounds += Pos;
		}
	}

#pragma endregion
}

void FPCGExEdgeDirectionSettings::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<FPCGExSortRuleConfig>* InSortingRules) const
{
	if (DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsSort)
	{
		FacadePreloader.Register<double>(InContext, DirSourceAttribute);
	}

	if (Sorter) { Sorter->RegisterBuffersDependencies(FacadePreloader); }
	else if (InSortingRules) { PCGExSorting::RegisterBuffersDependencies(InContext, FacadePreloader, *InSortingRules); }
}

bool FPCGExEdgeDirectionSettings::Init(
	FPCGExContext* InContext,
	const TSharedRef<PCGExData::FFacade>& InVtxDataFacade,
	const TArray<FPCGExSortRuleConfig>* InSortingRules)
{
	bAscendingDesired = DirectionChoice == EPCGExEdgeDirectionChoice::SmallestToGreatest;
	if (DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsSort)
	{
		if (!InSortingRules) { return false; }

		Sorter = MakeShared<PCGExSorting::PointSorter<true>>(InContext, InVtxDataFacade, *InSortingRules);
		Sorter->SortDirection = DirectionChoice == EPCGExEdgeDirectionChoice::GreatestToSmallest ? EPCGExSortDirection::Descending : EPCGExSortDirection::Ascending;
		if (!Sorter->Init()) { return false; }
	}
	return true;
}

bool FPCGExEdgeDirectionSettings::InitFromParent(
	FPCGExContext* InContext,
	const FPCGExEdgeDirectionSettings& ParentSettings,
	const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
{
	DirectionMethod = ParentSettings.DirectionMethod;
	DirectionChoice = ParentSettings.DirectionChoice;

	bAscendingDesired = ParentSettings.bAscendingDesired;
	Sorter = ParentSettings.Sorter;

	if (DirectionMethod == EPCGExEdgeDirectionMethod::EdgeDotAttribute)
	{
		EdgeDirReader = InEdgeDataFacade->GetScopedBroadcaster<FVector>(DirSourceAttribute);
		if (!EdgeDirReader)
		{
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Some edges don't have the specified DirSource Attribute \"{0}\"."), FText::FromName(DirSourceAttribute.GetName())));
			return false;
		}
	}

	return true;
}

bool FPCGExEdgeDirectionSettings::SortEndpoints(const PCGExCluster::FCluster* InCluster, PCGExGraph::FIndexedEdge& InEdge) const
{
	const uint32 Start = InEdge.Start;
	const uint32 End = InEdge.End;

	bool bAscending = true;

	if (DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsOrder)
	{
	}
	else if (DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsIndices)
	{
		bAscending = (Start < End);
	}
	else if (DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsSort)
	{
		bAscending = Sorter->Sort(Start, End);
	}
	else if (DirectionMethod == EPCGExEdgeDirectionMethod::EdgeDotAttribute)
	{
		// TODO : Might be faster to use the EndpointLookup with GetPos ?
		const FVector A = (InCluster->VtxPoints->GetData() + Start)->Transform.GetLocation();
		const FVector B = (InCluster->VtxPoints->GetData() + End)->Transform.GetLocation();

		const FVector& EdgeDir = (A - B).GetSafeNormal();
		const FVector& CounterDir = EdgeDirReader->Read(InEdge.EdgeIndex);
		bAscending = CounterDir.Dot(EdgeDir * -1) < CounterDir.Dot(EdgeDir); // TODO : Do we really need both dots?
	}

	if (bAscending != bAscendingDesired)
	{
		InEdge.Start = End;
		InEdge.End = Start;
		return true;
	}

	return false;
}
