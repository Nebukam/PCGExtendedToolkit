// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Clusters/PCGExCluster.h"

#include "Data/PCGExPointIO.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTags.h"
#include "Clusters/PCGExClusterCommon.h"
#include "Math/PCGExMathAxis.h"

namespace PCGExClusters
{
	FCluster::FCluster(const TSharedPtr<PCGExData::FPointIO>& InVtxIO, const TSharedPtr<PCGExData::FPointIO>& InEdgesIO, const TSharedPtr<PCGEx::FIndexLookup>& InNodeIndexLookup)
		: NodeIndexLookup(InNodeIndexLookup), VtxIO(InVtxIO), EdgesIO(InEdgesIO)
	{
		Nodes = MakeShared<TArray<FNode>>();
		Edges = MakeShared<TArray<FEdge>>();
		Bounds = FBox(ForceInit);

		VtxPoints = InVtxIO->GetIn();
	}

	FCluster::FCluster(const TSharedRef<FCluster>& OtherCluster, const TSharedPtr<PCGExData::FPointIO>& InVtxIO, const TSharedPtr<PCGExData::FPointIO>& InEdgesIO, const TSharedPtr<PCGEx::FIndexLookup>& InNodeIndexLookup, const bool bCopyNodes, const bool bCopyEdges, const bool bCopyLookup)
		: NodeIndexLookup(InNodeIndexLookup), VtxIO(InVtxIO), EdgesIO(InEdgesIO)
	{
		NodeIndexLookup = InNodeIndexLookup;
		VtxPoints = InVtxIO->GetIn();
		VtxTransforms = VtxPoints->GetConstTransformValueRange();

		bIsMirror = true;
		OriginalCluster = OtherCluster;

		NumRawVtx = InVtxIO->GetNum();
		NumRawEdges = InEdgesIO->GetNum();

		Bounds = OriginalCluster->Bounds;

		BoundedEdges = OriginalCluster->BoundedEdges;

		if (bCopyNodes)
		{
			const int32 NumNewNodes = OriginalCluster->Nodes->Num();

			Nodes = MakeShared<TArray<FNode>>();
			TArray<FNode>& NewNodes = *Nodes;
			TArray<FNode>& SourceNodes = *OriginalCluster->Nodes;

			PCGExArrayHelpers::InitArray(NewNodes, NumNewNodes);

			for (int i = 0; i < NumNewNodes; i++)
			{
				const FNode& NewNode = (NewNodes[i] = SourceNodes[i]);

				// Update index lookup
				NodeIndexLookup->GetMutable(NewNode.PointIndex) = NewNode.Index;
			}

			NodesDataPtr = Nodes->GetData();
		}
		else
		{
			Nodes = OriginalCluster->Nodes;
			NodesDataPtr = OriginalCluster->NodesDataPtr;

			// Update index lookup
			for (const FNode& Node : *Nodes) { NodeIndexLookup->GetMutable(Node.PointIndex) = Node.Index; }
		}

		if (bCopyEdges)
		{
			const int32 NumNewEdges = OriginalCluster->Edges->Num();

			Edges = MakeShared<TArray<FEdge>>();
			TArray<FEdge>& NewEdges = *Edges;
			TArray<FEdge>& SourceEdges = *OriginalCluster->Edges;

			PCGExArrayHelpers::InitArray(NewEdges, NumNewEdges);

			const int32 EdgeIOIndex = InEdgesIO->IOIndex;
			for (int i = 0; i < NumNewEdges; i++)
			{
				const FEdge& SourceEdge = SourceEdges[i];
				FEdge& NewEdge = (NewEdges[i] = SourceEdge);
				NewEdge.IOIndex = EdgeIOIndex;
			}

			BoundedEdges.Reset();
			EdgesDataPtr = Edges->GetData();
		}
		else
		{
			Edges = OriginalCluster->Edges;
			EdgesDataPtr = OriginalCluster->EdgesDataPtr;
		}
	}

	void FCluster::TConstVtxLookup::Dump(TArray<int32>& OutIndices) const
	{
		const int32 NumNodes = Num();
		OutIndices.SetNum(NumNodes);
		for (int i = 0; i < NumNodes; i++) { OutIndices[i] = NodesArray[i].PointIndex; }
	}

	void FCluster::TVtxLookup::Dump(TArray<int32>& OutIndices) const
	{
		const int32 NumNodes = Num();
		OutIndices.SetNum(NumNodes);
		for (int i = 0; i < NumNodes; i++) { OutIndices[i] = NodesArray[i].PointIndex; }
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
		BoundedEdges.Reset();
	}

	FCluster::~FCluster()
	{
	}

	bool FCluster::BuildFrom(const TMap<uint32, int32>& InEndpointsLookup, const TArray<int32>* InExpectedAdjacency)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCluster::BuildCluster);

		const TSharedPtr<PCGExData::FPointIO> PinnedVtxIO = VtxIO.Pin();
		const TSharedPtr<PCGExData::FPointIO> PinnedEdgesIO = EdgesIO.Pin();
		const int32 EdgeIOIndex = PinnedEdgesIO ? PinnedEdgesIO->IOIndex : -1;

		if (!PinnedVtxIO || !PinnedEdgesIO) { return false; }

		const UPCGBasePointData* InNodePoints = PinnedVtxIO->GetIn();
		VtxTransforms = InNodePoints->GetConstTransformValueRange();

		Nodes->Empty();
		Edges->Empty();

		const TUniquePtr<PCGExData::TArrayBuffer<int64>> EndpointsBuffer = MakeUnique<PCGExData::TArrayBuffer<int64>>(PinnedEdgesIO.ToSharedRef(), Labels::Attr_PCGExEdgeIdx);
		if (!EndpointsBuffer->InitForRead()) { return false; }

		NumRawVtx = InNodePoints->GetNumPoints();
		NumRawEdges = PinnedEdgesIO->GetNum();

		auto OnFail = [&]()
		{
			Nodes->Empty();
			Edges->Empty();
			return false;
		};

		const int32 NumEdges = PinnedEdgesIO->GetNum();

		PCGExArrayHelpers::InitArray(Edges, NumEdges);
		Nodes->Reserve(NumRawVtx);

		const TArray<int64>& Endpoints = *EndpointsBuffer->GetInValues().Get();

		for (int i = 0; i < NumEdges; i++)
		{
			uint32 A;
			uint32 B;
			PCGEx::H64(Endpoints[i], A, B);

			const int32* StartPointIndexPtr = InEndpointsLookup.Find(A);
			const int32* EndPointIndexPtr = InEndpointsLookup.Find(B);

			if ((!StartPointIndexPtr || !EndPointIndexPtr || *StartPointIndexPtr == *EndPointIndexPtr)) { return OnFail(); }

			const int32 StartNode = GetOrCreateNode_Unsafe(*StartPointIndexPtr);
			const int32 EndNode = GetOrCreateNode_Unsafe(*EndPointIndexPtr);

			(Nodes->GetData() + StartNode)->Link(EndNode, i);
			(Nodes->GetData() + EndNode)->Link(StartNode, i);

			*(Edges->GetData() + i) = FEdge(i, *StartPointIndexPtr, *EndPointIndexPtr, i, EdgeIOIndex);
		}

		if (InExpectedAdjacency)
		{
			for (const FNode& Node : (*Nodes))
			{
				// We care about removed connections, not new ones 
				if ((*InExpectedAdjacency)[Node.PointIndex] > Node.Num()) { return OnFail(); }
			}
		}

		Nodes->Shrink();
		Bounds = Bounds.ExpandBy(10);

		NodesDataPtr = Nodes->GetData();
		EdgesDataPtr = Edges->GetData();

		return true;
	}

	void FCluster::BuildFromSubgraphData(const TSharedPtr<PCGExData::FFacade>& InVtxFacade, const TSharedPtr<PCGExData::FFacade>& InEdgeFacade, const TArray<FEdge>& InEdges, const int32 InNumNodes)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCluster::BuildClusterFromSubgraph);

		TSharedPtr<FCluster> LocalPin = SharedThis(this);

		Bounds = FBox(ForceInit);

		NumRawVtx = InVtxFacade->Source->GetNum(PCGExData::EIOSide::Out);
		NumRawEdges = InEdgeFacade->Source->GetNum(PCGExData::EIOSide::Out);

		const UPCGBasePointData* SubVtxPoints = InVtxFacade->Source->GetOutIn();
		VtxTransforms = SubVtxPoints->GetConstTransformValueRange();

		Nodes->Reserve(InNumNodes);

		Edges->Reserve(NumRawEdges);
		Edges->Append(InEdges);

		TSparseArray<int32> TempLookup;
		TempLookup.Reserve(InNumNodes);

		for (const TArray<FEdge>& SubgraphEdges = *Edges.Get(); const FEdge& E : SubgraphEdges)
		{
			const int32 StartNode = GetOrCreateNode_Unsafe(TempLookup, E.Start);
			const int32 EndNode = GetOrCreateNode_Unsafe(TempLookup, E.End);

			(Nodes->GetData() + StartNode)->Link(EndNode, E.Index);
			(Nodes->GetData() + EndNode)->Link(StartNode, E.Index);
		}

		Bounds = Bounds.ExpandBy(10);

		NodesDataPtr = Nodes->GetData();
		EdgesDataPtr = Edges->GetData();
	}

	bool FCluster::IsValidWith(const TSharedRef<PCGExData::FPointIO>& InVtxIO, const TSharedRef<PCGExData::FPointIO>& InEdgesIO) const
	{
		return NumRawVtx == InVtxIO->GetNum() && NumRawEdges == InEdgesIO->GetNum();
	}

	bool FCluster::HasTag(const FString& InTag)
	{
		if (const TSharedPtr<PCGExData::FPointIO>& PinnedVtxIO = VtxIO.Pin()) { if (PinnedVtxIO->Tags->IsTagged(InTag)) { return true; } }
		if (const TSharedPtr<PCGExData::FPointIO>& PinnedEdgesIO = EdgesIO.Pin()) { if (PinnedEdgesIO->Tags->IsTagged(InTag)) { return true; } }
		return false;
	}

	FNode* FCluster::GetGuidedHalfEdge(const int32 Edge, const FVector& Guide, const FVector& Up) const
	{
		FNode* StartNode = GetEdgeStart(Edge);
		FNode* EndNode = GetEdgeEnd(Edge);

		if (StartNode->IsLeaf() && !EndNode->IsLeaf()) { return StartNode; }
		if (EndNode->IsLeaf() && !StartNode->IsLeaf()) { return EndNode; }

		const FVector& A = GetPos(StartNode);
		const FVector& B = GetPos(EndNode);
		const FVector& C = FMath::ClosestPointOnSegment(Guide, A, B);

		if (FVector::DotProduct((Guide - C).GetSafeNormal(), PCGExMath::GetNormalUp(A, B, Up)) < 0) { return GetEdgeStart(Edge); }
		return GetEdgeEnd(Edge);
	}

	double FCluster::EdgeDistToEdge(const FEdge* A, const FEdge* B, FVector& OutP1, FVector& OutP2) const
	{
		FMath::SegmentDistToSegment(GetStartPos(A), GetEndPos(A), GetStartPos(B), GetEndPos(B), OutP1, OutP2);

		return FVector::Dist(OutP1, OutP2);
	}

	double FCluster::EdgeDistToEdge(const int32 EdgeA, const int32 EdgeB, FVector& OutP1, FVector& OutP2) const
	{
		return EdgeDistToEdge(GetEdge(EdgeA), GetEdge(EdgeB), OutP1, OutP2);
	}

	double FCluster::EdgeDistToEdgeSquared(const FEdge* A, const FEdge* B, FVector& OutP1, FVector& OutP2) const
	{
		FMath::SegmentDistToSegment(GetStartPos(A), GetEndPos(A), GetStartPos(B), GetEndPos(B), OutP1, OutP2);

		return FVector::DistSquared(OutP1, OutP2);
	}

	double FCluster::EdgeDistToEdgeSquared(const int32 EdgeA, const int32 EdgeB, FVector& OutP1, FVector& OutP2) const
	{
		return EdgeDistToEdgeSquared(GetEdge(EdgeA), GetEdge(EdgeB), OutP1, OutP2);
	}

	FVector FCluster::GetDir(const int32 FromNode, const int32 ToNode) const
	{
		return (VtxTransforms[GetNodePointIndex(ToNode)].GetLocation() - VtxTransforms[GetNodePointIndex(FromNode)].GetLocation()).GetSafeNormal();
	}

	FVector FCluster::GetDir(const FNode& From, const FNode& To) const
	{
		return GetDir(From.Index, To.Index);
	}

	double FCluster::GetEdgeLength(const FEdge& InEdge) const
	{
		return FVector::Dist(VtxTransforms[InEdge.Start].GetLocation(), VtxTransforms[InEdge.End].GetLocation());
	}

	double FCluster::GetEdgeLengthSquared(const FEdge& InEdge) const
	{
		return FVector::DistSquared(VtxTransforms[InEdge.Start].GetLocation(), VtxTransforms[InEdge.End].GetLocation());
	}

	FVector FCluster::GetEdgeDir(const FEdge& InEdge) const
	{
		return (VtxTransforms[InEdge.Start].GetLocation() - VtxTransforms[InEdge.End].GetLocation()).GetSafeNormal();
	}

	FVector FCluster::GetEdgeDir(const int32 InEdgeIndex) const
	{
		const FEdge* Edge = EdgesDataPtr + InEdgeIndex;
		return (VtxTransforms[Edge->Start].GetLocation() - VtxTransforms[Edge->End].GetLocation()).GetSafeNormal();
	}

	FVector FCluster::GetEdgeDir(const FLink Lk) const
	{
		const FEdge* Edge = EdgesDataPtr + Lk.Edge;
		return (VtxTransforms[Edge->Start].GetLocation() - VtxTransforms[Edge->End].GetLocation()).GetSafeNormal();
	}

	FVector FCluster::GetEdgeDir(const int32 InEdgeIndex, const int32 InStartPtIndex) const
	{
		const FEdge* Edge = EdgesDataPtr + InEdgeIndex;
		return (VtxTransforms[InStartPtIndex].GetLocation() - VtxTransforms[Edge->Other(InStartPtIndex)].GetLocation()).GetSafeNormal();
	}

	FVector FCluster::GetEdgeDir(const FLink Lk, const int32 InStartPtIndex) const
	{
		const FEdge* Edge = EdgesDataPtr + Lk.Edge;
		return (VtxTransforms[InStartPtIndex].GetLocation() - VtxTransforms[Edge->Other(InStartPtIndex)].GetLocation()).GetSafeNormal();
	}

	TSharedPtr<PCGExOctree::FItemOctree> FCluster::GetNodeOctree()
	{
		if (!NodeOctree) { RebuildNodeOctree(); }
		return NodeOctree;
	}

	TSharedPtr<PCGExOctree::FItemOctree> FCluster::GetEdgeOctree()
	{
		if (!EdgeOctree) { RebuildEdgeOctree(); }
		return EdgeOctree;
	}

	void FCluster::RebuildNodeOctree()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCluster::RebuildNodeOctree);

		NodeOctree = MakeShared<PCGExOctree::FItemOctree>(Bounds.GetCenter(), (Bounds.GetExtent() + FVector(10)).Length());
		for (int i = 0; i < Nodes->Num(); i++)
		{
			const FNode* Node = NodesDataPtr + i;
			const PCGExData::FConstPoint Pt = PCGExData::FConstPoint(VtxPoints, Node->PointIndex);
			NodeOctree->AddElement(PCGExOctree::FItem(Node->Index, FBoxSphereBounds(Pt.GetLocalBounds().TransformBy(Pt.GetTransform()))));
		}
	}

	void FCluster::RebuildEdgeOctree()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCluster::RebuildEdgeOctree);

		check(Bounds.GetExtent().Length() != 0)

		EdgeOctree = MakeShared<PCGExOctree::FItemOctree>(Bounds.GetCenter(), (Bounds.GetExtent() + FVector(10)).Length());

		const int32 NumEdges = Edges->Num();

		if (!BoundedEdges)
		{
			BoundedEdges = MakeShared<TArray<FBoundedEdge>>();
			PCGExArrayHelpers::InitArray(BoundedEdges, NumEdges);

			TArray<FBoundedEdge>& BoundedEdgesRef = (*BoundedEdges);

			for (int i = 0; i < NumEdges; i++)
			{
				FBoundedEdge& NewBoundedEdge = BoundedEdgesRef[i];
				NewBoundedEdge = FBoundedEdge(this, i);
				EdgeOctree->AddElement(PCGExOctree::FItem(i, NewBoundedEdge.Bounds));
			}
		}
		else
		{
			const FBoundedEdge* BoundedEdgesDataPtr = BoundedEdges->GetData();
			for (int i = 0; i < NumEdges; i++) { EdgeOctree->AddElement(PCGExOctree::FItem(i, (BoundedEdgesDataPtr + i)->Bounds)); }
		}
	}

	void FCluster::RebuildOctree(const EPCGExClusterClosestSearchMode Mode, const bool bForceRebuild)
	{
		switch (Mode)
		{
		case EPCGExClusterClosestSearchMode::Vtx: if (NodeOctree && !bForceRebuild) { return; }
			RebuildNodeOctree();
			break;
		case EPCGExClusterClosestSearchMode::Edge: if (EdgeOctree && !bForceRebuild) { return; }
			RebuildEdgeOctree();
			break;
		default: ;
		}
	}

	void FCluster::GatherNodesPointIndices(TArray<int32>& OutValidNodesPointIndices, const bool bValidity) const
	{
		const TArray<FNode>& NodesRef = *Nodes.Get();

		OutValidNodesPointIndices.Reset();
		OutValidNodesPointIndices.Reserve(NodesRef.Num());
		const int8 Mask = bValidity ? 1 : 0;

		for (const FNode& Node : NodesRef) { if (Node.bValid == Mask) { OutValidNodesPointIndices.Add(Node.PointIndex); } }
	}

	int32 FCluster::FindClosestNode(const FVector& Position, EPCGExClusterClosestSearchMode Mode, const int32 MinNeighbors) const
	{
		switch (Mode)
		{
		default: ;
		case EPCGExClusterClosestSearchMode::Vtx: return FindClosestNode(Position, MinNeighbors);
		case EPCGExClusterClosestSearchMode::Edge: return FindClosestNodeFromEdge(Position, MinNeighbors);
		}
	}

	int32 FCluster::FindClosestNode(const FVector& Position, const int32 MinNeighbors) const
	{
		double MaxDistance = MAX_dbl;
		int32 ClosestIndex = -1;

		const TArray<FNode>& NodesRef = *Nodes;

		if (NodeOctree)
		{
			auto ProcessCandidate = [&](const PCGExOctree::FItem& Item)
			{
				const FNode& Node = NodesRef[Item.Index];
				if (MinNeighbors > 0 && Node.Num() < MinNeighbors) { return; }
				const double Dist = FVector::DistSquared(Position, GetPos(Node));
				if (Dist < MaxDistance)
				{
					MaxDistance = Dist;
					ClosestIndex = Node.Index;
				}
			};

			NodeOctree->FindNearbyElements(Position, ProcessCandidate);
		}
		else
		{
			for (const FNode& Node : (*Nodes))
			{
				if (MinNeighbors > 0 && Node.Num() < MinNeighbors) { continue; }
				const double Dist = FVector::DistSquared(Position, GetPos(Node));
				if (Dist < MaxDistance)
				{
					MaxDistance = Dist;
					ClosestIndex = Node.Index;
				}
			}
		}

		return ClosestIndex;
	}

	int32 FCluster::FindClosestNodeFromEdge(const FVector& Position, const int32 MinNeighbors) const
	{
		double MaxDistance = MAX_dbl;
		int32 ClosestIndex = -1;

		if (EdgeOctree)
		{
			auto ProcessCandidate = [&](const PCGExOctree::FItem& Item)
			{
				const double Dist = GetPointDistToEdgeSquared(Item.Index, Position);
				if (Dist < MaxDistance)
				{
					if (MinNeighbors > 0)
					{
						if (GetEdgeStart(Item.Index)->Links.Num() < MinNeighbors && GetEdgeEnd(Item.Index)->Links.Num() < MinNeighbors)
						{
							return;
						}
					}
					MaxDistance = Dist;
					ClosestIndex = Item.Index;
				}
			};

			EdgeOctree->FindNearbyElements(Position, ProcessCandidate);
		}
		else if (BoundedEdges)
		{
			for (const FBoundedEdge& Edge : (*BoundedEdges))
			{
				const double Dist = GetPointDistToEdgeSquared(Edge.Index, Position);
				if (Dist < MaxDistance)
				{
					if (MinNeighbors > 0)
					{
						if (GetEdgeStart(Edge.Index)->Links.Num() < MinNeighbors && GetEdgeEnd(Edge.Index)->Links.Num() < MinNeighbors){ continue; }
					}

					MaxDistance = Dist;
					ClosestIndex = Edge.Index;
				}
			}
		}
		else
		{
			for (const FEdge& Edge : (*Edges))
			{
				const double Dist = GetPointDistToEdgeSquared(Edge, Position);
				if (Dist < MaxDistance)
				{
					if (MinNeighbors > 0)
					{
						if (GetEdgeStart(Edge.Index)->Links.Num() < MinNeighbors && GetEdgeEnd(Edge.Index)->Links.Num() < MinNeighbors){ continue; }
					}

					MaxDistance = Dist;
					ClosestIndex = Edge.Index;
				}
			}
		}

		if (ClosestIndex == -1) { return -1; }

		const FEdge* Edge = GetEdge(ClosestIndex);
		const FNode* Start = GetEdgeStart(Edge);
		const FNode* End = GetEdgeEnd(Edge);

		ClosestIndex = FVector::DistSquared(Position, GetPos(Start)) < FVector::DistSquared(Position, GetPos(End)) ? Start->Index : End->Index;

		return ClosestIndex;
	}

	int32 FCluster::FindClosestEdge(const int32 InNodeIndex, const FVector& InPosition, const int32 MinNeighbors) const
	{
		if (!Nodes->IsValidIndex(InNodeIndex) || (NodesDataPtr + InNodeIndex)->IsEmpty()) { return -1; }
		const FNode& Node = *(NodesDataPtr + InNodeIndex);

		double BestDist = MAX_dbl;
		double BestDot = MAX_dbl;

		int32 BestIndex = -1;

		const FVector Position = GetPos(Node);
		const FVector SearchDirection = (GetPos(Node) - InPosition).GetSafeNormal();

		for (const FLink Lk : Node.Links)
		{
			if (MinNeighbors > 0 && GetNode(Lk)->Links.Num() < MinNeighbors) { continue; }

			FVector NPos = GetPos(Lk.Node);
			const double Dist = FMath::PointDistToSegmentSquared(InPosition, Position, NPos);
			if (Dist <= BestDist)
			{
				const double Dot = FVector::DotProduct(SearchDirection, (NPos - Position).GetSafeNormal());
				if (Dist == BestDist && Dot > BestDot) { continue; }
				BestDot = Dot;
				BestDist = Dist;
				BestIndex = Lk.Edge;
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
			auto ProcessCandidate = [&](const PCGExOctree::FItem& Item)
			{
				if (NodesRef[Item.Index].Num() < MinNeighborCount) { return; }
				if (const double Dist = FMath::PointDistToSegmentSquared(Position, NodePosition, GetPos(Item.Index)); Dist < LastDist)
				{
					LastDist = Dist;
					Result = Item.Index;
				}
			};

			NodeOctree->FindNearbyElements(Position, ProcessCandidate);
		}
		else
		{
			for (const FLink Lk : Node.Links)
			{
				if (NodesRef[Lk.Node].Num() < MinNeighborCount) { continue; }
				if (const double Dist = FMath::PointDistToSegmentSquared(Position, NodePosition, GetPos(Lk)); Dist < LastDist)
				{
					LastDist = Dist;
					Result = Lk.Node;
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
			auto ProcessCandidate = [&](const PCGExOctree::FItem& Item)
			{
				if (NodesRef[Item.Index].Num() < MinNeighborCount) { return; }
				if (Exclusion.Contains(Item.Index)) { return; }
				if (const double Dist = FMath::PointDistToSegmentSquared(Position, NodePosition, GetPos(Item.Index)); Dist < LastDist)
				{
					LastDist = Dist;
					Result = Item.Index;
				}
			};

			NodeOctree->FindNearbyElements(Position, ProcessCandidate);
		}
		else
		{
			for (const FLink Lk : Node.Links)
			{
				if (NodesRef[Lk.Node].Num() < MinNeighborCount) { continue; }
				if (Exclusion.Contains(Lk.Node)) { continue; }
				if (const double Dist = FMath::PointDistToSegmentSquared(Position, NodePosition, GetPos(Lk)); Dist < LastDist)
				{
					LastDist = Dist;
					Result = Lk.Node;
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

		const int32 NumEdges = Edges->Num();
		double Min = MAX_dbl;
		double Max = MIN_dbl;
		EdgeLengths->SetNumUninitialized(NumEdges);

		for (int i = 0; i < NumEdges; i++)
		{
			const FEdge* Edge = GetEdge(i);
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

		for (const FLink Lk : RootNode.Links)
		{
			if (OutIndices.Contains(Lk.Node)) { continue; }

			OutIndices.Add(Lk.Node);
			if (NextDepth > 0) { GetConnectedNodes(Lk.Node, OutIndices, NextDepth); }
		}
	}

	void FCluster::GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth, const TSet<int32>& Skip) const
	{
		const int32 NextDepth = SearchDepth - 1;
		const FNode& RootNode = (*Nodes)[FromIndex];

		for (const FLink Lk : RootNode.Links)
		{
			if (Skip.Contains(Lk.Node) || OutIndices.Contains(Lk.Node)) { continue; }

			OutIndices.Add(Lk.Node);
			if (NextDepth > 0) { GetConnectedNodes(Lk.Node, OutIndices, NextDepth, Skip); }
		}
	}

	void FCluster::GetConnectedEdges(const int32 FromNodeIndex, TArray<int32>& OutNodeIndices, TArray<int32>& OutEdgeIndices, const int32 SearchDepth) const
	{
		const int32 NextDepth = SearchDepth - 1;
		const FNode& RootNode = (*Nodes)[FromNodeIndex];

		for (const FLink Lk : RootNode.Links)
		{
			if (OutNodeIndices.Contains(Lk.Node)) { continue; }
			if (OutEdgeIndices.Contains(Lk.Edge)) { continue; }

			OutNodeIndices.Add(Lk.Node);
			OutEdgeIndices.Add(Lk.Edge);

			if (NextDepth > 0) { GetConnectedEdges(Lk.Node, OutNodeIndices, OutEdgeIndices, NextDepth); }
		}
	}

	void FCluster::GetConnectedEdges(const int32 FromNodeIndex, TArray<int32>& OutNodeIndices, TArray<int32>& OutEdgeIndices, const int32 SearchDepth, const TSet<int32>& SkipNodes, const TSet<int32>& SkipEdges) const
	{
		const int32 NextDepth = SearchDepth - 1;
		const FNode& RootNode = (*Nodes)[FromNodeIndex];

		for (const FLink Lk : RootNode.Links)
		{
			if (SkipNodes.Contains(Lk.Node) || OutNodeIndices.Contains(Lk.Node)) { continue; }
			if (SkipEdges.Contains(Lk.Edge) || OutEdgeIndices.Contains(Lk.Edge)) { continue; }

			OutNodeIndices.Add(Lk.Node);
			OutEdgeIndices.Add(Lk.Edge);

			if (NextDepth > 0) { GetConnectedEdges(Lk.Node, OutNodeIndices, OutEdgeIndices, NextDepth, SkipNodes, SkipEdges); }
		}
	}

	FVector FCluster::GetClosestPointOnEdge(const int32 FromIndex, const int32 ToIndex, const FVector& Position) const
	{
		return FMath::ClosestPointOnSegment(Position, GetPos(FromIndex), GetPos(ToIndex));
	}

	FVector FCluster::GetClosestPointOnEdge(const FEdge& InEdge, const FVector& Position) const
	{
		return FMath::ClosestPointOnSegment(Position, VtxTransforms[InEdge.Start].GetLocation(), VtxTransforms[InEdge.End].GetLocation());
	}

	FVector FCluster::GetClosestPointOnEdge(const int32 EdgeIndex, const FVector& Position) const
	{
		return GetClosestPointOnEdge(*(EdgesDataPtr + EdgeIndex), Position);
	}

	double FCluster::GetPointDistToEdgeSquared(const FEdge& InEdge, const FVector& Position) const
	{
		return FMath::PointDistToSegmentSquared(Position, VtxTransforms[InEdge.Start].GetLocation(), VtxTransforms[InEdge.End].GetLocation());
	}

	double FCluster::GetPointDistToEdgeSquared(const int32 EdgeIndex, const FVector& Position) const
	{
		return GetPointDistToEdgeSquared(*(EdgesDataPtr + EdgeIndex), Position);
	}

	FVector FCluster::GetCentroid(const int32 NodeIndex) const
	{
		const FNode* Node = (NodesDataPtr + NodeIndex);
		FVector Centroid = FVector::ZeroVector;
		for (const FLink Lk : Node->Links) { Centroid += GetPos(Lk.Node); }
		return Centroid / static_cast<double>(Node->Num());
	}

	void FCluster::GetValidEdges(TArray<FEdge>& OutValidEdges) const
	{
		const TSharedPtr<PCGExData::FPointIO> PinnedEdgesIO = EdgesIO.Pin();
		OutValidEdges.Reserve(Edges->Num());

		const int32 IOIndex = PinnedEdgesIO ? PinnedEdgesIO->IOIndex : -1;

		for (const FEdge& Edge : (*Edges))
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

		for (const FLink Lk : Node.Links)
		{
			if (NodesRef[Lk.Node].Num() < MinNeighborCount) { continue; }
			if (const double Dot = FVector::DotProduct(Direction, GetDir(NodeIndex, Lk.Node)); Dot > LastDot)
			{
				LastDot = Dot;
				Result = Lk.Node;
			}
		}

		return Result;
	}

	TSharedPtr<TArray<FBoundedEdge>> FCluster::GetBoundedEdges(const bool bBuild)
	{
		{
			FReadScopeLock ReadScopeLock(ClusterLock);
			if (BoundedEdges) { return BoundedEdges; }
		}
		{
			FWriteScopeLock WriteScopeLock(ClusterLock);

			BoundedEdges = MakeShared<TArray<FBoundedEdge>>();
			PCGExArrayHelpers::InitArray(BoundedEdges, Edges->Num());

			TArray<FBoundedEdge>& ExpandedEdgesRef = (*BoundedEdges);
			if (bBuild) { for (int i = 0; i < BoundedEdges->Num(); i++) { ExpandedEdgesRef[i] = FBoundedEdge(this, i); } } // Ooof
		}

		return BoundedEdges;
	}

	void FCluster::ExpandEdges(PCGExMT::FTaskManager* TaskManager)
	{
		if (BoundedEdges) { return; }

		PCGEX_ASYNC_GROUP_CHKD_VOID(TaskManager, ExpandEdgesTask);

		BoundedEdges = MakeShared<TArray<FBoundedEdge>>();
		PCGExArrayHelpers::InitArray(BoundedEdges, Edges->Num());

		ExpandEdgesTask->OnSubLoopStartCallback = [PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
		{
			PCGEX_ASYNC_THIS

			TArray<FBoundedEdge>& ExpandedEdgesRef = (*This->BoundedEdges);
			const FCluster* Cluster = This.Get();
			PCGEX_SCOPE_LOOP(i) { ExpandedEdgesRef[i] = FBoundedEdge(Cluster, i); }
		};

		ExpandEdgesTask->StartSubLoops(Edges->Num(), 256);
	}

	int32 FCluster::GetOrCreateNode_Unsafe(const int32 PointIndex)
	{
		int32 NodeIndex = NodeIndexLookup->Get(PointIndex);

		if (NodeIndex != -1) { return NodeIndex; }

		NodeIndex = Nodes->Add(FNode(Nodes->Num(), PointIndex));
		NodeIndexLookup->GetMutable(PointIndex) = NodeIndex;
		Bounds += VtxTransforms[PointIndex].GetLocation();

		return NodeIndex;
	}

	int32 FCluster::GetOrCreateNode_Unsafe(TSparseArray<int32>& InLookup, const int32 PointIndex)
	{
		if (InLookup.IsValidIndex(PointIndex))
		{
			return InLookup[PointIndex];
		}

		const int32 NodeIndex = Nodes->Add(FNode(Nodes->Num(), PointIndex));
		InLookup.Insert(PointIndex, NodeIndex);

		Bounds += VtxTransforms[PointIndex].GetLocation();

		return NodeIndex;
	}
}
