﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCluster.h"

#include "PCGExMath.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTag.h"

namespace PCGExCluster
{
#pragma region FNode

	FNode::FNode(const int32 InNodeIndex, const int32 InPointIndex)
		: PCGExGraph::FNode(InNodeIndex, InPointIndex)
	{
	}

	FVector FNode::GetCentroid(const FCluster* InCluster) const
	{
		if (Links.IsEmpty()) { return InCluster->GetPos(Index); }

		FVector Centroid = FVector::ZeroVector;
		const int32 NumPoints = Links.Num();

		for (int i = 0; i < NumPoints; i++) { Centroid += InCluster->GetPos(Links[i].Node); }

		if (Links.Num() < 2)
		{
			Centroid += InCluster->GetPos(Index);
			return Centroid / 2;
		}

		Centroid /= static_cast<double>(NumPoints);

		return Centroid;
	}

	FVector FNode::ComputeNormal(const FCluster* InCluster, const TArray<FAdjacencyData>& AdjacencyData) const
	{
		const int32 NumNeighbors = AdjacencyData.Num();

		FVector OutNormal = FVector::ZeroVector;

		if (AdjacencyData.IsEmpty()) { return OutNormal; }

		for (const FAdjacencyData& A : AdjacencyData)
		{
			FVector Position = InCluster->GetPos(Index);
			OutNormal += PCGExMath::GetNormal(InCluster->GetPos(A.NodeIndex), Position, Position + FVector::ForwardVector);
		}

		OutNormal /= NumNeighbors;

		return OutNormal;
	}

	int32 FNode::ValidEdges(const FCluster* InCluster)
	{
		int32 ValidNum = 0;
		for (const FLink Lk : Links) { if (InCluster->GetEdge(Lk.Edge)->bValid) { ValidNum++; } }
		return ValidNum;
	}

	bool FNode::HasAnyValidEdges(const FCluster* InCluster)
	{
		for (const FLink Lk : Links) { if (InCluster->GetEdge(Lk.Edge)->bValid) { return true; } }
		return false;
	}

#pragma endregion

#pragma region FCluster

	FCluster::FCluster(const TSharedPtr<PCGExData::FPointIO>& InVtxIO, const TSharedPtr<PCGExData::FPointIO>& InEdgesIO,
	                   const TSharedPtr<PCGEx::FIndexLookup>& InNodeIndexLookup):
		NodeIndexLookup(InNodeIndexLookup), VtxIO(InVtxIO), EdgesIO(InEdgesIO)
	{
		Nodes = MakeShared<TArray<FNode>>();
		Edges = MakeShared<TArray<FEdge>>();
		Bounds = FBox(ForceInit);

		VtxPoints = InVtxIO->GetIn();
	}

	FCluster::FCluster(const TSharedRef<FCluster>& OtherCluster,
	                   const TSharedPtr<PCGExData::FPointIO>& InVtxIO,
	                   const TSharedPtr<PCGExData::FPointIO>& InEdgesIO,
	                   const TSharedPtr<PCGEx::FIndexLookup>& InNodeIndexLookup,
	                   const bool bCopyNodes, const bool bCopyEdges, const bool bCopyLookup):
		NodeIndexLookup(InNodeIndexLookup), VtxIO(InVtxIO), EdgesIO(InEdgesIO)
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

			PCGEx::InitArray(NewNodes, NumNewNodes);

			for (int i = 0; i < NumNewNodes; i++)
			{
				const FNode& NewNode = (NewNodes[i] = SourceNodes[i]);

				// Update index lookup
				NodeIndexLookup->GetMutable(NewNode.PointIndex) = NewNode.Index;
			}
		}
		else
		{
			Nodes = OriginalCluster->Nodes;

			// Update index lookup
			for (const FNode& Node : *Nodes) { NodeIndexLookup->GetMutable(Node.PointIndex) = Node.Index; }
		}

		if (bCopyEdges)
		{
			const int32 NumNewEdges = OriginalCluster->Edges->Num();

			Edges = MakeShared<TArray<FEdge>>();
			TArray<FEdge>& NewEdges = *Edges;
			TArray<FEdge>& SourceEdges = *OriginalCluster->Edges;

			PCGEx::InitArray(NewEdges, NumNewEdges);

			const int32 EdgeIOIndex = InEdgesIO->IOIndex;
			for (int i = 0; i < NumNewEdges; i++)
			{
				const FEdge& SourceEdge = SourceEdges[i];
				FEdge& NewEdge = (NewEdges[i] = SourceEdge);
				NewEdge.IOIndex = EdgeIOIndex;
			}

			BoundedEdges.Reset();
		}
		else
		{
			Edges = OriginalCluster->Edges;
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

	bool FCluster::BuildFrom(
		const TMap<uint32, int32>& InEndpointsLookup,
		const TArray<int32>* InExpectedAdjacency)
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

		const TUniquePtr<PCGExData::TArrayBuffer<int64>> EndpointsBuffer = MakeUnique<PCGExData::TArrayBuffer<int64>>(PinnedEdgesIO.ToSharedRef(), PCGExGraph::Attr_PCGExEdgeIdx);
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

		PCGEx::InitArray(Edges, NumEdges);
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

		return true;
	}

	void FCluster::BuildFrom(const TSharedRef<PCGExGraph::FSubGraph>& SubGraph)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCluster::BuildClusterFromSubgraph);

		TSharedPtr<FCluster> LocalPin = SharedThis(this);

		Bounds = FBox(ForceInit);

		NumRawVtx = SubGraph->VtxDataFacade->Source->GetNum(PCGExData::EIOSide::Out);
		NumRawEdges = SubGraph->EdgesDataFacade->Source->GetNum(PCGExData::EIOSide::Out);

		const UPCGBasePointData* SubVtxPoints = SubGraph->VtxDataFacade->Source->GetOutIn();
		VtxTransforms = SubVtxPoints->GetConstTransformValueRange();

		Nodes->Reserve(SubGraph->Nodes.Num());

		Edges->Reserve(NumRawEdges);
		Edges->Append(SubGraph->FlattenedEdges);

		TSparseArray<int32> TempLookup;
		TempLookup.Reserve(SubGraph->Nodes.Num());

		const int32 NumEdges = Edges->Num();

		for (int i = 0; i < NumEdges; i++)
		{
			const FEdge* E = Edges->GetData() + i;
			const int32 StartNode = GetOrCreateNode_Unsafe(TempLookup, E->Start);
			const int32 EndNode = GetOrCreateNode_Unsafe(TempLookup, E->End);

			(Nodes->GetData() + StartNode)->Link(EndNode, E->Index);
			(Nodes->GetData() + EndNode)->Link(StartNode, E->Index);
		}

		Bounds = Bounds.ExpandBy(10);
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
		FMath::SegmentDistToSegment(
			GetStartPos(A), GetEndPos(A),
			GetStartPos(B), GetEndPos(B),
			OutP1, OutP2);

		return FVector::Dist(OutP1, OutP2);
	}

	double FCluster::EdgeDistToEdge(const int32 EdgeA, const int32 EdgeB, FVector& OutP1, FVector& OutP2) const
	{
		return EdgeDistToEdge(GetEdge(EdgeA), GetEdge(EdgeB), OutP1, OutP2);
	}

	double FCluster::EdgeDistToEdgeSquared(const FEdge* A, const FEdge* B, FVector& OutP1, FVector& OutP2) const
	{
		FMath::SegmentDistToSegment(
			GetStartPos(A), GetEndPos(A),
			GetStartPos(B), GetEndPos(B),
			OutP1, OutP2);

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
		const FEdge* Edge = Edges->GetData() + InEdgeIndex;
		return (VtxTransforms[Edge->Start].GetLocation() - VtxTransforms[Edge->End].GetLocation()).GetSafeNormal();
	}

	FVector FCluster::GetEdgeDir(const FLink Lk) const
	{
		const FEdge* Edge = Edges->GetData() + Lk.Edge;
		return (VtxTransforms[Edge->Start].GetLocation() - VtxTransforms[Edge->End].GetLocation()).GetSafeNormal();
	}

	FVector FCluster::GetEdgeDir(const int32 InEdgeIndex, const int32 InStartPtIndex) const
	{
		const FEdge* Edge = Edges->GetData() + InEdgeIndex;
		return (VtxTransforms[InStartPtIndex].GetLocation() - VtxTransforms[Edge->Other(InStartPtIndex)].GetLocation()).GetSafeNormal();
	}

	FVector FCluster::GetEdgeDir(const FLink Lk, const int32 InStartPtIndex) const
	{
		const FEdge* Edge = Edges->GetData() + Lk.Edge;
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
			const FNode* Node = Nodes->GetData() + i;
			const PCGExData::FConstPoint Pt = PCGExData::FConstPoint(VtxPoints, Node->PointIndex);
			NodeOctree->AddElement(PCGExOctree::FItem(Node->Index, FBoxSphereBounds(Pt.GetLocalBounds().TransformBy(Pt.GetTransform()))));
		}
	}

	void FCluster::RebuildEdgeOctree()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCluster::RebuildEdgeOctree);

		check(Bounds.GetExtent().Length() != 0)

		EdgeOctree = MakeShared<PCGExOctree::FItemOctree>(Bounds.GetCenter(), (Bounds.GetExtent() + FVector(10)).Length());

		if (!BoundedEdges)
		{
			BoundedEdges = MakeShared<TArray<FBoundedEdge>>();
			PCGEx::InitArray(BoundedEdges, Edges->Num());

			TArray<FBoundedEdge>& BoundedEdgesRef = (*BoundedEdges);

			for (int i = 0; i < Edges->Num(); i++)
			{
				const FBoundedEdge& NewBoundedEdge = (BoundedEdgesRef[i] = FBoundedEdge(this, i));
				EdgeOctree->AddElement(PCGExOctree::FItem(i, NewBoundedEdge.Bounds));
			}
		}
		else
		{
			for (int i = 0; i < Edges->Num(); i++)
			{
				EdgeOctree->AddElement(PCGExOctree::FItem(i, (BoundedEdges->GetData() + i)->Bounds));
			}
		}
	}

	void FCluster::RebuildOctree(const EPCGExClusterClosestSearchMode Mode, const bool bForceRebuild)
	{
		switch (Mode)
		{
		case EPCGExClusterClosestSearchMode::Vtx:
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

	void FCluster::GatherNodesPointIndices(TArray<int32>& OutValidNodesPointIndices, const bool bValidity) const
	{
		const TArray<FNode>& NodesRef = *Nodes.Get();

		OutValidNodesPointIndices.Reset();
		OutValidNodesPointIndices.Reserve(NodesRef.Num());
		const int8 Mask = bValidity ? 1 : 0;

		for (const FNode& Node : NodesRef) { if (Node.bValid == Mask) { OutValidNodesPointIndices.Add(Node.PointIndex); } }
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
			for (const FLink Lk : Node.Links)
			{
				if (NodesRef[Lk.Node].Num() < MinNeighborCount) { continue; }
				if (const double Dist = FMath::PointDistToSegmentSquared(Position, NodePosition, GetPos(Lk));
					Dist < LastDist)
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
			for (const FLink Lk : Node.Links)
			{
				if (NodesRef[Lk.Node].Num() < MinNeighborCount) { continue; }
				if (Exclusion.Contains(Lk.Node)) { continue; }
				if (const double Dist = FMath::PointDistToSegmentSquared(Position, NodePosition, GetPos(Lk));
					Dist < LastDist)
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
		return FMath::ClosestPointOnSegment(
			Position,
			VtxTransforms[InEdge.Start].GetLocation(),
			VtxTransforms[InEdge.End].GetLocation());
	}

	FVector FCluster::GetClosestPointOnEdge(const int32 EdgeIndex, const FVector& Position) const
	{
		return GetClosestPointOnEdge(*(Edges->GetData() + EdgeIndex), Position);
	}

	double FCluster::GetPointDistToEdgeSquared(const FEdge& InEdge, const FVector& Position) const
	{
		return FMath::PointDistToSegmentSquared(
			Position,
			VtxTransforms[InEdge.Start].GetLocation(),
			VtxTransforms[InEdge.End].GetLocation());
	}

	double FCluster::GetPointDistToEdgeSquared(const int32 EdgeIndex, const FVector& Position) const
	{
		return GetPointDistToEdgeSquared(*(Edges->GetData() + EdgeIndex), Position);
	}

	FVector FCluster::GetCentroid(const int32 NodeIndex) const
	{
		const FNode* Node = (Nodes->GetData() + NodeIndex);
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
			if (const double Dot = FVector::DotProduct(Direction, GetDir(NodeIndex, Lk.Node));
				Dot > LastDot)
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
			PCGEx::InitArray(BoundedEdges, Edges->Num());

			TArray<FBoundedEdge>& ExpandedEdgesRef = (*BoundedEdges);
			if (bBuild) { for (int i = 0; i < BoundedEdges->Num(); i++) { ExpandedEdgesRef[i] = FBoundedEdge(this, i); } } // Ooof
		}

		return BoundedEdges;
	}

	void FCluster::ExpandEdges(PCGExMT::FTaskManager* AsyncManager)
	{
		if (BoundedEdges) { return; }

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ExpandEdgesTask);

		BoundedEdges = MakeShared<TArray<FBoundedEdge>>();
		PCGEx::InitArray(BoundedEdges, Edges->Num());

		ExpandEdgesTask->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
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

	void GetAdjacencyData(const FCluster* InCluster, FNode& InNode, TArray<FAdjacencyData>& OutData)
	{
		const int32 NumAdjacency = InNode.Num();
		const FVector NodePosition = InCluster->GetPos(InNode);
		OutData.Reserve(NumAdjacency);
		for (int i = 0; i < NumAdjacency; i++)
		{
			const FLink Lk = InNode.Links[i];

			const FNode* OtherNode = InCluster->Nodes->GetData() + Lk.Node;
			const FVector OtherPosition = InCluster->GetPos(OtherNode);

			FAdjacencyData& Data = OutData.Emplace_GetRef();
			Data.NodeIndex = Lk.Node;
			Data.NodePointIndex = OtherNode->PointIndex;
			Data.EdgeIndex = Lk.Edge;
			Data.Direction = (NodePosition - OtherPosition).GetSafeNormal();
			Data.Length = FVector::Dist(NodePosition, OtherPosition);
		}
	}

	FBoundedEdge::FBoundedEdge(const FCluster* Cluster, const int32 InEdgeIndex):
		Index(InEdgeIndex),
		Bounds(
			FBoxSphereBounds(
				FSphere(
					FMath::Lerp(Cluster->GetStartPos(InEdgeIndex), Cluster->GetEndPos(InEdgeIndex), 0.5),
					Cluster->GetDist(InEdgeIndex) * 0.5)))
	{
	}

	FBoundedEdge::FBoundedEdge()
		: Index(-1), Bounds(FBoxSphereBounds(ForceInit))
	{
	}

#pragma endregion
}

bool FPCGExNodeSelectionDetails::WithinDistance(const FVector& NodePosition, const FVector& TargetPosition) const
{
	if (MaxDistance <= 0) { return true; }
	return FVector::Distance(NodePosition, TargetPosition) < MaxDistance;
}
