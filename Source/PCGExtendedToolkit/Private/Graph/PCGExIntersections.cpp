// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExIntersections.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExCluster.h"

namespace PCGExGraph
{
	FVector FUnionNode::UpdateCenter(const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata, const TSharedPtr<PCGExData::FPointIOCollection>& IOGroup)
	{
		Center = FVector::ZeroVector;
		const TSharedPtr<PCGExData::FUnionData> UnionData = InUnionMetadata->Get(Index);

		const double Divider = UnionData->ItemHashSet.Num();

		for (const uint64 H : UnionData->ItemHashSet)
		{
			Center += IOGroup->Pairs[PCGEx::H64A(H)]->GetInPoint(PCGEx::H64B(H)).Transform.GetLocation();
		}

		Center /= Divider;
		return Center;
	}

	TSharedPtr<FUnionNode> FUnionGraph::InsertPoint(const FPCGPoint& Point, const int32 IOIndex, const int32 PointIndex)
	{
		const FVector Origin = Point.Transform.GetLocation();
		TSharedPtr<FUnionNode> Node;

		if (!Octree)
		{
			const uint32 GridKey = FuseDetails.GetGridKey(Origin);
			TSharedPtr<FUnionNode>* NodePtr;
			{
				FReadScopeLock ReadScopeLock(UnionLock);
				NodePtr = GridTree.Find(GridKey);
			}

			if (NodePtr)
			{
				Node = *NodePtr;
				NodesUnion->Append(Node->Index, IOIndex, PointIndex);
				return Node;
			}

			{
				FWriteScopeLock WriteLock(UnionLock);
				NodePtr = GridTree.Find(GridKey); // Make sure there hasn't been an insert while locking

				if (NodePtr)
				{
					Node = *NodePtr;
					NodesUnion->Append(Node->Index, IOIndex, PointIndex);
					return Node;
				}

				Node = MakeShared<FUnionNode>(Point, Origin, Nodes.Num());
				Nodes.Add(Node);
				NodesUnion->NewEntry(IOIndex, PointIndex);
				GridTree.Add(GridKey, Node);
			}

			return Node;
		}

		{
			// Read lock starts
			int32 NodeIndex = -1;

			FReadScopeLock ReadScopeLock(UnionLock);

			if (FuseDetails.bComponentWiseTolerance)
			{
				Octree->FindFirstElementWithBoundsTest(
					FuseDetails.GetOctreeBox(Origin), [&](const FUnionNode* ExistingNode)
					{
						if (FuseDetails.IsWithinToleranceComponentWise(Point, ExistingNode->Point))
						{
							NodeIndex = ExistingNode->Index;
							return false;
						}
						return true;
					});
			}
			else
			{
				Octree->FindFirstElementWithBoundsTest(
					FuseDetails.GetOctreeBox(Origin), [&](const FUnionNode* ExistingNode)
					{
						if (FuseDetails.IsWithinTolerance(Point, ExistingNode->Point))
						{
							NodeIndex = ExistingNode->Index;
							return false;
						}
						return true;
					});
			}

			if (NodeIndex != -1)
			{
				NodesUnion->Append(NodeIndex, IOIndex, PointIndex);
				return Nodes[NodeIndex];
			}

			// Read lock ends
		}

		{
			// Write lock start
			FWriteScopeLock WriteScopeLock(UnionLock);

			Node = MakeShared<FUnionNode>(Point, Origin, Nodes.Num());
			Nodes.Add(Node);
			Octree->AddElement(Node.Get());
			NodesUnion->NewEntry(IOIndex, PointIndex);
		}

		return Node;
	}

	TSharedPtr<FUnionNode> FUnionGraph::InsertPoint_Unsafe(const FPCGPoint& Point, const int32 IOIndex, const int32 PointIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionGraph::InsertPoint_Unsafe);

		const FVector Origin = Point.Transform.GetLocation();
		TSharedPtr<FUnionNode> Node;

		if (!Octree)
		{
			const uint32 GridKey = FuseDetails.GetGridKey(Origin);

			if (TSharedPtr<FUnionNode>* NodePtr = GridTree.Find(GridKey))
			{
				Node = *NodePtr;
				NodesUnion->Append(Node->Index, IOIndex, PointIndex);
				return Node;
			}

			Node = MakeShared<FUnionNode>(Point, Origin, Nodes.Num());
			Nodes.Add(Node);
			NodesUnion->NewEntry(IOIndex, PointIndex);
			GridTree.Add(GridKey, Node);

			return Node;
		}

		int32 NodeIndex = -1;

		if (FuseDetails.bComponentWiseTolerance)
		{
			Octree->FindFirstElementWithBoundsTest(
				FuseDetails.GetOctreeBox(Origin), [&](const FUnionNode* ExistingNode)
				{
					if (FuseDetails.IsWithinToleranceComponentWise(Point, ExistingNode->Point))
					{
						NodeIndex = ExistingNode->Index;
						return false;
					}
					return true;
				});
		}
		else
		{
			Octree->FindFirstElementWithBoundsTest(
				FuseDetails.GetOctreeBox(Origin), [&](const FUnionNode* ExistingNode)
				{
					if (FuseDetails.IsWithinTolerance(Point, ExistingNode->Point))
					{
						NodeIndex = ExistingNode->Index;
						return false;
					}
					return true;
				});
		}

		if (NodeIndex != -1)
		{
			NodesUnion->Append(NodeIndex, IOIndex, PointIndex);
			return Nodes[NodeIndex];
		}

		Node = MakeShared<FUnionNode>(Point, Origin, Nodes.Num());
		Nodes.Add(Node);
		Octree->AddElement(Node.Get());
		NodesUnion->NewEntry(IOIndex, PointIndex);

		return Node;
	}

	TSharedPtr<PCGExData::FUnionData> FUnionGraph::InsertEdge(const FPCGPoint& From, const int32 FromIOIndex, const int32 FromPointIndex, const FPCGPoint& To, const int32 ToIOIndex, const int32 ToPointIndex, const int32 EdgeIOIndex, const int32 EdgePointIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionData::InsertEdge);

		const TSharedPtr<FUnionNode> StartVtx = InsertPoint(From, FromIOIndex, FromPointIndex);
		const TSharedPtr<FUnionNode> EndVtx = InsertPoint(To, ToIOIndex, ToPointIndex);

		if (StartVtx == EndVtx) { return nullptr; } // Edge got fused entirely

		StartVtx->Add(EndVtx->Index);
		EndVtx->Add(StartVtx->Index);

		TSharedPtr<PCGExData::FUnionData> EdgeUnion;

		const uint64 H = PCGEx::H64U(StartVtx->Index, EndVtx->Index);

		{
			FReadScopeLock ReadLockEdges(EdgesLock);
			if (const FEdge* Edge = Edges.Find(H)) { EdgeUnion = EdgesUnion->Entries[Edge->Index]; }
		}

		if (EdgeUnion)
		{
			if (EdgeIOIndex == -1) { EdgeUnion->Add(EdgeIOIndex, EdgeUnion->Num()); } // Abstract tracking to get valid union data
			else { EdgeUnion->Add(EdgeIOIndex, EdgePointIndex); }
			return EdgeUnion;
		}

		{
			FWriteScopeLock WriteLockEdges(EdgesLock);

			if (const FEdge* Edge = Edges.Find(H)) { EdgeUnion = EdgesUnion->Entries[Edge->Index]; }

			if (EdgeUnion)
			{
				if (EdgeIOIndex == -1) { EdgeUnion->Add(EdgeIOIndex, EdgeUnion->Num()); } // Abstract tracking to get valid union data
				else { EdgeUnion->Add(EdgeIOIndex, EdgePointIndex); }
				return EdgeUnion;
			}

			EdgeUnion = EdgesUnion->NewEntry(EdgeIOIndex, EdgePointIndex == -1 ? 0 : EdgePointIndex);
			Edges.Add(H, FEdge(Edges.Num(), StartVtx->Index, EndVtx->Index));
		}

		return EdgeUnion;
	}

	TSharedPtr<PCGExData::FUnionData> FUnionGraph::InsertEdge_Unsafe(const FPCGPoint& From, const int32 FromIOIndex, const int32 FromPointIndex, const FPCGPoint& To, const int32 ToIOIndex, const int32 ToPointIndex, const int32 EdgeIOIndex, const int32 EdgePointIndex)
	{
		const TSharedPtr<FUnionNode> StartVtx = InsertPoint_Unsafe(From, FromIOIndex, FromPointIndex);
		const TSharedPtr<FUnionNode> EndVtx = InsertPoint_Unsafe(To, ToIOIndex, ToPointIndex);

		if (StartVtx == EndVtx) { return nullptr; } // Edge got fused entirely

		StartVtx->Adjacency.Add(EndVtx->Index);
		EndVtx->Adjacency.Add(StartVtx->Index);

		const uint64 H = PCGEx::H64U(StartVtx->Index, EndVtx->Index);
		TSharedPtr<PCGExData::FUnionData> EdgeUnion;

		if (EdgeIOIndex == -1)
		{
			// Abstract edge management, so we have some valid metadata even tho there are no valid input edges
			// So EdgeIOIndex will be invalid, be we can still track union data
			if (const FEdge* Edge = Edges.Find(H))
			{
				EdgeUnion = EdgesUnion->Entries[Edge->Index];
				EdgeUnion->Add(EdgeIOIndex, EdgeUnion->Num());
			}
			else
			{
				EdgeUnion = EdgesUnion->NewEntry(EdgeIOIndex, 0);
				Edges.Add(H, FEdge(Edges.Num(), StartVtx->Index, EndVtx->Index));
			}
		}
		else
		{
			// Concrete edge management, we have valild input edges			
			if (const FEdge* Edge = Edges.Find(H))
			{
				EdgeUnion = EdgesUnion->Entries[Edge->Index];
				EdgeUnion->Add(EdgeIOIndex, EdgePointIndex);
			}
			else
			{
				EdgeUnion = EdgesUnion->NewEntry(EdgeIOIndex, EdgePointIndex);
				Edges.Add(H, FEdge(Edges.Num(), StartVtx->Index, EndVtx->Index));
			}
		}

		return EdgeUnion;
	}

	void FUnionGraph::GetUniqueEdges(TSet<uint64>& OutEdges)
	{
		OutEdges.Empty(Nodes.Num() * 4);
		for (const TSharedPtr<FUnionNode>& Node : Nodes)
		{
			for (const int32 OtherNodeIndex : Node->Adjacency)
			{
				const uint64 Hash = PCGEx::H64U(Node->Index, OtherNodeIndex);
				OutEdges.Add(Hash);
			}
		}
	}

	void FUnionGraph::GetUniqueEdges(TArray<FEdge>& OutEdges)
	{
		const int32 NumEdges = Edges.Num();
		OutEdges.SetNumUninitialized(NumEdges);
		for (const TPair<uint64, FEdge>& Pair : Edges) { OutEdges[Pair.Value.Index] = Pair.Value; }
	}

	void FUnionGraph::WriteNodeMetadata(const TSharedPtr<FGraph>& InGraph) const
	{
		InGraph->NodeMetadata.Reserve(Nodes.Num());

		for (const TSharedPtr<FUnionNode>& Node : Nodes)
		{
			const TSharedPtr<PCGExData::FUnionData>& UnionData = NodesUnion->Entries[Node->Index];
			FGraphNodeMetadata& NodeMeta = InGraph->GetOrCreateNodeMetadata_Unsafe(Node->Index);
			NodeMeta.UnionSize = UnionData->Num();
		}
	}

	void FUnionGraph::WriteEdgeMetadata(const TSharedPtr<FGraph>& InGraph) const
	{
		const int32 NumEdges = Edges.Num();
		InGraph->EdgeMetadata.Reserve(NumEdges);

		for (int i = 0; i < NumEdges; i++)
		{
			const TSharedPtr<PCGExData::FUnionData>& UnionData = EdgesUnion->Entries[i];
			FGraphEdgeMetadata& EdgeMetadata = InGraph->GetOrCreateEdgeMetadata_Unsafe(i);
			EdgeMetadata.UnionSize = UnionData->Num();
		}
		/*
		for (const TPair<uint64, FIndexedEdge>& Pair : Edges)
		{
			const int32 Index = Pair.Value.EdgeIndex;
			const TUniquePtr<PCGExData::FUnionData>& UnionData = EdgesUnion->Items[Index];
			FGraphEdgeMetadata& EdgeMetadata = FGraphEdgeMetadata::GetOrCreate(Index, nullptr, OutMetadata);
			EdgeMetadata.UnionSize = UnionData->Num();
		}
		*/
	}

	FPointEdgeIntersections::FPointEdgeIntersections(
		const TSharedPtr<FGraph>& InGraph,
		const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		const FPCGExPointEdgeIntersectionDetails* InDetails)
		: PointIO(InPointIO), Graph(InGraph), Details(InDetails)
	{
		const TArray<FPCGPoint>& Points = InPointIO->GetOutIn()->GetPoints();

		const int32 NumEdges = InGraph->Edges.Num();
		Edges.SetNum(NumEdges);

		for (const FEdge& Edge : InGraph->Edges)
		{
			if (!Edge.bValid) { continue; }
			Edges[Edge.Index].Init(
				Edge.Index,
				Points[Edge.Start].Transform.GetLocation(),
				Points[Edge.End].Transform.GetLocation(),
				Details->FuseDetails.Tolerance);
		}
	}

	void FPointEdgeIntersections::Insert()
	{
		FEdge NewEdge = FEdge{};

		for (FPointEdgeProxy& PointEdgeProxy : Edges)
		{
			if (PointEdgeProxy.CollinearPoints.IsEmpty()) { continue; }

			const FEdge& SplitEdge = Graph->Edges[PointEdgeProxy.EdgeIndex];
			const FGraphEdgeMetadata* ParentEdgeMeta = Graph->FindEdgeMetadata_Unsafe(SplitEdge.Index);

			int32 NodeIndex = -1;

			int32 PrevIndex = SplitEdge.Start;
			for (const FPESplit Split : PointEdgeProxy.CollinearPoints)
			{
				NodeIndex = Split.NodeIndex;

				Graph->InsertEdge(PrevIndex, NodeIndex, NewEdge, SplitEdge.IOIndex); //TODO: IOIndex required
				Graph->AddNodeAndEdgeMetadata_Unsafe(NodeIndex, NewEdge.Index, ParentEdgeMeta, EPCGExIntersectionType::PointEdge);

				PrevIndex = NodeIndex;

				if (Details->bSnapOnEdge)
				{
					PointIO->GetMutablePoint(Graph->Nodes[Split.NodeIndex].PointIndex).Transform.SetLocation(Split.ClosestPoint);
				}
			}

			Graph->InsertEdge(PrevIndex, SplitEdge.End, NewEdge, SplitEdge.IOIndex); // Insert last edge
			Graph->AddEdgeMetadata_Unsafe(NewEdge.Index, ParentEdgeMeta, EPCGExIntersectionType::PointEdge);
		}
	}

	void FPointEdgeIntersections::BlendIntersection(const int32 Index, PCGExDataBlending::FMetadataBlender* Blender) const
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPointEdgeIntersections::BlendIntersection);

		const FPointEdgeProxy& PointEdgeProxy = Edges[Index];

		if (PointEdgeProxy.CollinearPoints.IsEmpty()) { return; }

		const FEdge& SplitEdge = Graph->Edges[PointEdgeProxy.EdgeIndex];

		const PCGExData::FPointRef A = PointIO->GetOutPointRef(SplitEdge.Start);
		const PCGExData::FPointRef B = PointIO->GetOutPointRef(SplitEdge.End);

		for (const FPESplit Split : PointEdgeProxy.CollinearPoints)
		{
			const PCGExData::FPointRef Target = PointIO->GetOutPointRef(Graph->Nodes[Split.NodeIndex].PointIndex);
			FPCGPoint& Pt = PointIO->GetMutablePoint(Target.Index);

			FVector PreBlendLocation = Pt.Transform.GetLocation();

			Blender->PrepareForBlending(Target);
			Blender->Blend(A, B, Target, 0.5);
			Blender->CompleteBlending(Target, 2, 1);

			Pt.Transform.SetLocation(PreBlendLocation);
		}
	}

	FEdgeEdgeIntersections::FEdgeEdgeIntersections(
		const TSharedPtr<FGraph>& InGraph,
		const TSharedPtr<FUnionGraph>& InUnionGraph,
		const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		const FPCGExEdgeEdgeIntersectionDetails* InDetails)
		: PointIO(InPointIO), Graph(InGraph), Details(InDetails)
	{
		const TArray<FPCGPoint>& Points = InPointIO->GetOutIn()->GetPoints();

		const int32 NumEdges = InGraph->Edges.Num();
		Edges.SetNum(NumEdges);

		Octree = MakeUnique<FEdgeEdgeProxyOctree>(InUnionGraph->Bounds.GetCenter(), InUnionGraph->Bounds.GetExtent().Length() + (Details->Tolerance * 2));

		for (const FEdge& Edge : InGraph->Edges)
		{
			if (!Edge.bValid) { continue; }
			Edges[Edge.Index].Init(
				Edge.Index,
				Points[Edge.Start].Transform.GetLocation(),
				Points[Edge.End].Transform.GetLocation(),
				Details->Tolerance);

			Octree->AddElement(&Edges[Edge.Index]);
		}
	}

	bool FEdgeEdgeIntersections::InsertNodes() const
	{
		if (Crossings.IsEmpty()) { return false; }

		// Insert new nodes
		Graph->AddNodes(Crossings.Num());

		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
		const int32 StartIndex = MutablePoints.Num();
		MutablePoints.SetNum(Graph->Nodes.Num());

		UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;
		for (int i = StartIndex; i < MutablePoints.Num(); i++) { Metadata->InitializeOnSet(MutablePoints[i].MetadataEntry); }

		return true;
	}

	void FEdgeEdgeIntersections::InsertEdges()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FEdgeEdgeIntersections::Insert);

		FEdge NewEdge = FEdge{};

		for (FEdgeEdgeProxy& EdgeProxy : Edges)
		{
			if (EdgeProxy.Intersections.IsEmpty()) { continue; }

			const FEdge SplitEdge = Graph->Edges[EdgeProxy.EdgeIndex];
			const FGraphEdgeMetadata* ParentEdgeMeta = Graph->FindEdgeMetadata_Unsafe(SplitEdge.Index);

			int32 NodeIndex = -1;
			int32 PrevIndex = SplitEdge.Start;

			for (const int32 IntersectionIndex : EdgeProxy.Intersections)
			{
				const FEECrossing& Crossing = Crossings[IntersectionIndex];

				NodeIndex = Crossing.NodeIndex;

				Graph->InsertEdge_Unsafe(PrevIndex, NodeIndex, NewEdge, SplitEdge.IOIndex); //BUG: this is the wrong edge IOIndex
				Graph->AddNodeAndEdgeMetadata_Unsafe(NodeIndex, NewEdge.Index, ParentEdgeMeta, EPCGExIntersectionType::EdgeEdge);

				PrevIndex = NodeIndex;
			}

			Graph->InsertEdge_Unsafe(PrevIndex, SplitEdge.End, NewEdge, SplitEdge.IOIndex); // Insert last edge
			Graph->AddEdgeMetadata_Unsafe(NewEdge.Index, ParentEdgeMeta, EPCGExIntersectionType::EdgeEdge);
		}
	}

	void FEdgeEdgeIntersections::BlendIntersection(const int32 Index, const TSharedRef<PCGExDataBlending::FMetadataBlender>& Blender) const
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FEdgeEdgeIntersections::BlendIntersection);

		const FEECrossing& Crossing = Crossings[Index];

		const int32 Target = Graph->Nodes[Crossing.NodeIndex].PointIndex;
		Blender->PrepareForBlending(Target);

		const int32 A1 = Graph->Nodes[Graph->Edges[Crossing.EdgeA].Start].PointIndex;
		const int32 A2 = Graph->Nodes[Graph->Edges[Crossing.EdgeA].End].PointIndex;
		const int32 B1 = Graph->Nodes[Graph->Edges[Crossing.EdgeB].Start].PointIndex;
		const int32 B2 = Graph->Nodes[Graph->Edges[Crossing.EdgeB].End].PointIndex;

		Blender->Blend(Target, A1, Target, Crossing.Split.TimeA);
		Blender->Blend(Target, A2, Target, 1 - Crossing.Split.TimeA);
		Blender->Blend(Target, B1, Target, Crossing.Split.TimeB);
		Blender->Blend(Target, B2, Target, 1 - Crossing.Split.TimeB);

		Blender->CompleteBlending(Target, 4, 2);

		PointIO->GetMutablePoint(Target).Transform.SetLocation(Crossing.Split.Center);
	}
}
