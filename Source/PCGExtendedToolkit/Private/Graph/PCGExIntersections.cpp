// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExIntersections.h"

#include "IntVectorTypes.h"
#include "PCGExPointsProcessor.h"
#include "Graph/PCGExCluster.h"

namespace PCGExGraph
{
	FVector FUnionNode::UpdateCenter(const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata, const TSharedPtr<PCGExData::FPointIOCollection>& IOGroup)
	{
		Center = FVector::ZeroVector;
		PCGExData::FUnionData* UnionData = InUnionMetadata->Get(Index);

		const double Divider = UnionData->ItemHashSet.Num();

		for (const uint64 H : UnionData->ItemHashSet)
		{
			Center += IOGroup->Pairs[PCGEx::H64A(H)]->GetInPoint(PCGEx::H64B(H)).Transform.GetLocation();
		}

		Center /= Divider;
		return Center;
	}

	FUnionNode* FUnionGraph::InsertPoint(const FPCGPoint& Point, const int32 IOIndex, const int32 PointIndex)
	{
		const FVector Origin = Point.Transform.GetLocation();
		FUnionNode* Node;

		if (!Octree)
		{
			const uint32 GridKey = FuseDetails.GetGridKey(Origin);
			FUnionNode** NodePtr;
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

				Node = Nodes.Add_GetRef(new FUnionNode(Point, Origin, Nodes.Num()));
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

			Node = Nodes.Add_GetRef(new FUnionNode(Point, Origin, Nodes.Num()));
			Octree->AddElement(Node);
			NodesUnion->NewEntry(IOIndex, PointIndex);
		}

		return Node;
	}

	FUnionNode* FUnionGraph::InsertPointUnsafe(const FPCGPoint& Point, const int32 IOIndex, const int32 PointIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionGraph::InsertPointUnsafe);

		const FVector Origin = Point.Transform.GetLocation();
		FUnionNode* Node;

		if (!Octree)
		{
			const uint32 GridKey = FuseDetails.GetGridKey(Origin);

			if (FUnionNode** NodePtr = GridTree.Find(GridKey))
			{
				Node = *NodePtr;
				NodesUnion->Append(Node->Index, IOIndex, PointIndex);
				return Node;
			}

			Node = Nodes.Add_GetRef(new FUnionNode(Point, Origin, Nodes.Num()));
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

		Node = Nodes.Add_GetRef(new FUnionNode(Point, Origin, Nodes.Num()));
		Octree->AddElement(Node);
		NodesUnion->NewEntry(IOIndex, PointIndex);

		return Node;
	}

	PCGExData::FUnionData* FUnionGraph::InsertEdge(const FPCGPoint& From, const int32 FromIOIndex, const int32 FromPointIndex, const FPCGPoint& To, const int32 ToIOIndex, const int32 ToPointIndex, const int32 EdgeIOIndex, const int32 EdgePointIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionData::InsertEdge);

		FUnionNode* StartVtx = InsertPoint(From, FromIOIndex, FromPointIndex);
		FUnionNode* EndVtx = InsertPoint(To, ToIOIndex, ToPointIndex);

		if (StartVtx == EndVtx) { return nullptr; } // Edge got fused entirely

		StartVtx->Add(EndVtx->Index);
		EndVtx->Add(StartVtx->Index);

		PCGExData::FUnionData* EdgeUnion = nullptr;

		const uint64 H = PCGEx::H64U(StartVtx->Index, EndVtx->Index);

		{
			FReadScopeLock ReadLockEdges(EdgesLock);
			if (const FIndexedEdge* Edge = Edges.Find(H)) { EdgeUnion = EdgesUnion->Entries[Edge->EdgeIndex]; }
		}

		if (EdgeUnion)
		{
			if (EdgeIOIndex == -1) { EdgeUnion->Add(EdgeIOIndex, EdgeUnion->Num()); } // Abstract tracking to get valid union data
			else { EdgeUnion->Add(EdgeIOIndex, EdgePointIndex); }
			return EdgeUnion;
		}

		{
			FWriteScopeLock WriteLockEdges(EdgesLock);

			if (const FIndexedEdge* Edge = Edges.Find(H)) { EdgeUnion = EdgesUnion->Entries[Edge->EdgeIndex]; }

			if (EdgeUnion)
			{
				if (EdgeIOIndex == -1) { EdgeUnion->Add(EdgeIOIndex, EdgeUnion->Num()); } // Abstract tracking to get valid union data
				else { EdgeUnion->Add(EdgeIOIndex, EdgePointIndex); }
				return EdgeUnion;
			}

			EdgeUnion = EdgesUnion->NewEntry(EdgeIOIndex, EdgePointIndex == -1 ? 0 : EdgePointIndex);
			Edges.Add(H, FIndexedEdge(Edges.Num(), StartVtx->Index, EndVtx->Index));
		}

		return EdgeUnion;
	}

	PCGExData::FUnionData* FUnionGraph::InsertEdgeUnsafe(const FPCGPoint& From, const int32 FromIOIndex, const int32 FromPointIndex, const FPCGPoint& To, const int32 ToIOIndex, const int32 ToPointIndex, const int32 EdgeIOIndex, const int32 EdgePointIndex)
	{
		FUnionNode* StartVtx = InsertPointUnsafe(From, FromIOIndex, FromPointIndex);
		FUnionNode* EndVtx = InsertPointUnsafe(To, ToIOIndex, ToPointIndex);

		if (StartVtx == EndVtx) { return nullptr; } // Edge got fused entirely

		StartVtx->Adjacency.Add(EndVtx->Index);
		EndVtx->Adjacency.Add(StartVtx->Index);

		const uint64 H = PCGEx::H64U(StartVtx->Index, EndVtx->Index);
		PCGExData::FUnionData* EdgeUnion = nullptr;

		if (EdgeIOIndex == -1)
		{
			// Abstract edge management, so we have some valid metadata even tho there are no valid input edges
			// So EdgeIOIndex will be invalid, be we can still track union data
			if (const FIndexedEdge* Edge = Edges.Find(H))
			{
				EdgeUnion = EdgesUnion->Entries[Edge->EdgeIndex];
				EdgeUnion->Add(EdgeIOIndex, EdgeUnion->Num());
			}
			else
			{
				EdgeUnion = EdgesUnion->NewEntry(EdgeIOIndex, 0);
				Edges.Add(H, FIndexedEdge(Edges.Num(), StartVtx->Index, EndVtx->Index));
			}
		}
		else
		{
			// Concrete edge management, we have valild input edges			
			if (const FIndexedEdge* Edge = Edges.Find(H))
			{
				EdgeUnion = EdgesUnion->Entries[Edge->EdgeIndex];
				EdgeUnion->Add(EdgeIOIndex, EdgePointIndex);
			}
			else
			{
				EdgeUnion = EdgesUnion->NewEntry(EdgeIOIndex, EdgePointIndex);
				Edges.Add(H, FIndexedEdge(Edges.Num(), StartVtx->Index, EndVtx->Index));
			}
		}

		return EdgeUnion;
	}

	void FUnionGraph::GetUniqueEdges(TSet<uint64>& OutEdges)
	{
		OutEdges.Empty(Nodes.Num() * 4);
		for (const FUnionNode* Node : Nodes)
		{
			for (const int32 OtherNodeIndex : Node->Adjacency)
			{
				const uint64 Hash = PCGEx::H64U(Node->Index, OtherNodeIndex);
				OutEdges.Add(Hash);
			}
		}
	}

	void FUnionGraph::GetUniqueEdges(TArray<FIndexedEdge>& OutEdges)
	{
		const int32 NumEdges = Edges.Num();
		OutEdges.SetNumUninitialized(NumEdges);
		for (const TPair<uint64, FIndexedEdge>& Pair : Edges) { OutEdges[Pair.Value.EdgeIndex] = Pair.Value; }
	}

	void FUnionGraph::WriteNodeMetadata(const TSharedPtr<FGraph>& InGraph) const
	{
		InGraph->NodeMetadata.Reserve(Nodes.Num());

		for (const FUnionNode* Node : Nodes)
		{
			const PCGExData::FUnionData* UnionData = NodesUnion->Entries[Node->Index];
			FGraphNodeMetadata& NodeMeta = InGraph->GetOrCreateNodeMetadataUnsafe(Node->Index);
			NodeMeta.UnionSize = UnionData->Num();
		}
	}

	void FUnionGraph::WriteEdgeMetadata(const TSharedPtr<FGraph>& InGraph) const
	{
		const int32 NumEdges = Edges.Num();
		InGraph->EdgeMetadata.Reserve(NumEdges);

		for (int i = 0; i < NumEdges; i++)
		{
			const PCGExData::FUnionData* UnionData = EdgesUnion->Entries[i];
			FGraphEdgeMetadata& EdgeMetadata = InGraph->GetOrCreateEdgeMetadataUnsafe(i);
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

		for (const FIndexedEdge& Edge : InGraph->Edges)
		{
			if (!Edge.bValid) { continue; }
			Edges[Edge.EdgeIndex].Init(
				Edge.EdgeIndex,
				Points[Edge.Start].Transform.GetLocation(),
				Points[Edge.End].Transform.GetLocation(),
				Details->FuseDetails.Tolerance);
		}
	}

	void FPointEdgeIntersections::Insert()
	{
		FIndexedEdge NewEdge = FIndexedEdge{};

		for (FPointEdgeProxy& PointEdgeProxy : Edges)
		{
			if (PointEdgeProxy.CollinearPoints.IsEmpty()) { continue; }

			const FIndexedEdge& SplitEdge = Graph->Edges[PointEdgeProxy.EdgeIndex];
			const FGraphEdgeMetadata* ParentEdgeMeta = Graph->FindEdgeMetadataUnsafe(SplitEdge.EdgeIndex);

			int32 NodeIndex = -1;

			int32 PrevIndex = SplitEdge.Start;
			for (const FPESplit Split : PointEdgeProxy.CollinearPoints)
			{
				NodeIndex = Split.NodeIndex;

				Graph->InsertEdge(PrevIndex, NodeIndex, NewEdge, SplitEdge.IOIndex); //TODO: IOIndex required
				Graph->AddNodeAndEdgeMetadata(NodeIndex, NewEdge.EdgeIndex, ParentEdgeMeta, EPCGExIntersectionType::PointEdge);

				PrevIndex = NodeIndex;

				if (Details->bSnapOnEdge)
				{
					PointIO->GetMutablePoint(Graph->Nodes[Split.NodeIndex].PointIndex).Transform.SetLocation(Split.ClosestPoint);
				}
			}

			Graph->InsertEdge(PrevIndex, SplitEdge.End, NewEdge, SplitEdge.IOIndex); // Insert last edge
			Graph->AddEdgeMetadata(NewEdge.EdgeIndex, ParentEdgeMeta, EPCGExIntersectionType::PointEdge);
		}
	}

	void FPointEdgeIntersections::BlendIntersection(const int32 Index, PCGExDataBlending::FMetadataBlender* Blender) const
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPointEdgeIntersections::BlendIntersection);

		const FPointEdgeProxy& PointEdgeProxy = Edges[Index];

		if (PointEdgeProxy.CollinearPoints.IsEmpty()) { return; }

		const FIndexedEdge& SplitEdge = Graph->Edges[PointEdgeProxy.EdgeIndex];

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

		for (const FIndexedEdge& Edge : InGraph->Edges)
		{
			if (!Edge.bValid) { continue; }
			Edges[Edge.EdgeIndex].Init(
				Edge.EdgeIndex,
				Points[Edge.Start].Transform.GetLocation(),
				Points[Edge.End].Transform.GetLocation(),
				Details->Tolerance);

			Octree->AddElement(&Edges[Edge.EdgeIndex]);
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

		FIndexedEdge NewEdge = FIndexedEdge{};

		for (FEdgeEdgeProxy& EdgeProxy : Edges)
		{
			if (EdgeProxy.Intersections.IsEmpty()) { continue; }

			const FIndexedEdge SplitEdge = Graph->Edges[EdgeProxy.EdgeIndex];
			const FGraphEdgeMetadata* ParentEdgeMeta = Graph->FindEdgeMetadataUnsafe(SplitEdge.EdgeIndex);

			int32 NodeIndex = -1;
			int32 PrevIndex = SplitEdge.Start;

			for (const int32 IntersectionIndex : EdgeProxy.Intersections)
			{
				const FEECrossing& Crossing = Crossings[IntersectionIndex];

				NodeIndex = Crossing.NodeIndex;

				Graph->InsertEdgeUnsafe(PrevIndex, NodeIndex, NewEdge, SplitEdge.IOIndex); //BUG: this is the wrong edge IOIndex
				Graph->AddNodeAndEdgeMetadata(NodeIndex, NewEdge.EdgeIndex, ParentEdgeMeta, EPCGExIntersectionType::EdgeEdge);

				PrevIndex = NodeIndex;
			}

			Graph->InsertEdgeUnsafe(PrevIndex, SplitEdge.End, NewEdge, SplitEdge.IOIndex); // Insert last edge
			Graph->AddEdgeMetadata(NewEdge.EdgeIndex, ParentEdgeMeta, EPCGExIntersectionType::EdgeEdge);
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
