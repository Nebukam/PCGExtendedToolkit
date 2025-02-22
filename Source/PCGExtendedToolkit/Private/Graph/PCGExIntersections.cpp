// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExIntersections.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExCluster.h"

namespace PCGExGraph
{
	FUnionNode::FUnionNode(const FPCGPoint& InPoint, const FVector& InCenter, const int32 InIndex)
		: Point(InPoint), Center(InCenter), Index(InIndex)
	{
		Adjacency.Empty();
		Bounds = FBoxSphereBounds(InPoint.GetLocalBounds().TransformBy(InPoint.Transform));
	}

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

	void FUnionNode::Add(const int32 InAdjacency)
	{
		FWriteScopeLock WriteScopeLock(AdjacencyLock);
		Adjacency.Add(InAdjacency);
	}

	FUnionGraph::FUnionGraph(const FPCGExFuseDetails& InFuseDetails, const FBox& InBounds)
		: FuseDetails(InFuseDetails), Bounds(InBounds)
	{
		Nodes.Empty();
		Edges.Empty();

		FuseDetails.Init();

		NodesUnion = MakeShared<PCGExData::FUnionMetadata>();
		EdgesUnion = MakeShared<PCGExData::FUnionMetadata>();

		if (InFuseDetails.FuseMethod == EPCGExFuseMethod::Octree) { Octree = MakeUnique<FUnionNodeOctree>(Bounds.GetCenter(), Bounds.GetExtent().Length() + 10); }
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


				if (NodePtr)
				{
					Node = *NodePtr;
					NodesUnion->Append(Node->Index, IOIndex, PointIndex);
					return Node;
				}
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
			// Write lock starts
			FWriteScopeLock WriteScopeLock(UnionLock);

			PCGExMath::FClosestLocation ClosestNode(Origin);

			if (FuseDetails.bComponentWiseTolerance)
			{
				Octree->FindElementsWithBoundsTest(
					FuseDetails.GetOctreeBox(Origin), [&](const FUnionNode* ExistingNode)
					{
						if (FuseDetails.IsWithinToleranceComponentWise(Point, ExistingNode->Point))
						{
							ClosestNode.Push(Point.Transform.GetLocation(), ExistingNode->Index);
							return false;
						}
						return true;
					});
			}
			else
			{
				Octree->FindElementsWithBoundsTest(
					FuseDetails.GetOctreeBox(Origin), [&](const FUnionNode* ExistingNode)
					{
						if (FuseDetails.IsWithinTolerance(Point, ExistingNode->Point))
						{
							ClosestNode.Push(Point.Transform.GetLocation(), ExistingNode->Index);
							return false;
						}
						return true;
					});
			}

			if (ClosestNode.bValid)
			{
				NodesUnion->Append(ClosestNode.Index, IOIndex, PointIndex);
				return Nodes[ClosestNode.Index];
			}

			// Write lock ends
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

		PCGExMath::FClosestLocation ClosestNode(Origin);

		if (FuseDetails.bComponentWiseTolerance)
		{
			Octree->FindElementsWithBoundsTest(
				FuseDetails.GetOctreeBox(Origin), [&](const FUnionNode* ExistingNode)
				{
					if (FuseDetails.IsWithinToleranceComponentWise(Point, ExistingNode->Point))
					{
						ClosestNode.Push(Point.Transform.GetLocation(), ExistingNode->Index);
						return false;
					}
					return true;
				});
		}
		else
		{
			Octree->FindElementsWithBoundsTest(
				FuseDetails.GetOctreeBox(Origin), [&](const FUnionNode* ExistingNode)
				{
					if (FuseDetails.IsWithinTolerance(Point, ExistingNode->Point))
					{
						ClosestNode.Push(Point.Transform.GetLocation(), ExistingNode->Index);
						return false;
					}
					return true;
				});
		}

		if (ClosestNode.bValid)
		{
			NodesUnion->Append(ClosestNode.Index, IOIndex, PointIndex);
			return Nodes[ClosestNode.Index];
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

			if (EdgeUnion)
			{
				if (EdgeIOIndex == -1) { EdgeUnion->Add(EdgeIOIndex, EdgeUnion->Num()); } // Abstract tracking to get valid union data
				else { EdgeUnion->Add(EdgeIOIndex, EdgePointIndex); }
				return EdgeUnion;
			}
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

	FPointEdgeProxy::FPointEdgeProxy(const int32 InEdgeIndex, const FVector& InStart, const FVector& InEnd, const double Tolerance)
	{
		Init(InEdgeIndex, InStart, InEnd, Tolerance);
	}

	void FPointEdgeProxy::Init(const int32 InEdgeIndex, const FVector& InStart, const FVector& InEnd, const double Tolerance)
	{
		CollinearPoints.Empty();

		Start = InStart;
		End = InEnd;

		EdgeIndex = InEdgeIndex;
		ToleranceSquared = Tolerance * Tolerance;

		Box = FBox(ForceInit);
		Box += Start;
		Box += End;
		Box = Box.ExpandBy(Tolerance);

		LengthSquared = FVector::DistSquared(Start, End);
	}

	bool FPointEdgeProxy::FindSplit(const FVector& Position, FPESplit& OutSplit) const
	{
		const FVector ClosestPoint = FMath::ClosestPointOnSegment(Position, Start, End);

		if ((ClosestPoint - Start).IsNearlyZero() || (ClosestPoint - End).IsNearlyZero()) { return false; } // Overlap endpoint
		if (FVector::DistSquared(ClosestPoint, Position) >= ToleranceSquared) { return false; }             // Too far

		OutSplit.ClosestPoint = ClosestPoint;
		OutSplit.Time = (FVector::DistSquared(Start, ClosestPoint) / LengthSquared);
		return true;
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

	void FPointEdgeIntersections::Add(const int32 EdgeIndex, const FPESplit& Split)
	{
		FWriteScopeLock WriteLock(InsertionLock);
		Edges[EdgeIndex].CollinearPoints.AddUnique(Split);
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

	void FindCollinearNodes(const TSharedPtr<FPointEdgeIntersections>& InIntersections, const int32 EdgeIndex, const UPCGPointData* PointsData)
	{
		const TArray<FPCGPoint>& Points = PointsData->GetPoints();

		const FPointEdgeProxy& Edge = InIntersections->Edges[EdgeIndex];
		const TSharedPtr<FGraph> Graph = InIntersections->Graph;

		const FEdge& IEdge = Graph->Edges[EdgeIndex];
		FPESplit Split = FPESplit{};

		if (!InIntersections->Details->bEnableSelfIntersection)
		{
			const int32 RootIndex = InIntersections->Graph->FindEdgeMetadata_Unsafe(Edge.EdgeIndex)->RootIndex;
			const TSet<int32>& RootIOIndices = Graph->EdgesUnion->Entries[RootIndex]->IOIndices;

			auto ProcessPointRef = [&](const FPCGPointRef& PointRef)
			{
				const ptrdiff_t PointIndex = PointRef.Point - Points.GetData();

				if (!Points.IsValidIndex(static_cast<int32>(PointIndex))) { return; }
				const FNode& Node = InIntersections->Graph->Nodes[PointIndex];

				if (!Node.bValid) { return; }

				const FVector Position = Points[Node.PointIndex].Transform.GetLocation();

				if (!Edge.Box.IsInside(Position)) { return; }
				if (IEdge.Start == Node.PointIndex || IEdge.End == Node.PointIndex) { return; }
				if (!Edge.FindSplit(Position, Split)) { return; }

				if (Graph->NodesUnion->IOIndexOverlap(Node.Index, RootIOIndices)) { return; }

				Split.NodeIndex = Node.Index;
				InIntersections->Add(EdgeIndex, Split);
			};

			PointsData->GetOctree().FindElementsWithBoundsTest(Edge.Box, ProcessPointRef);
		}
		else
		{
			auto ProcessPointRef = [&](const FPCGPointRef& PointRef)
			{
				const ptrdiff_t PointIndex = PointRef.Point - Points.GetData();

				if (!Points.IsValidIndex(static_cast<int32>(PointIndex))) { return; }
				const FNode& Node = InIntersections->Graph->Nodes[PointIndex];

				if (!Node.bValid) { return; }

				const FVector Position = Points[Node.PointIndex].Transform.GetLocation();

				if (!Edge.Box.IsInside(Position)) { return; }
				if (IEdge.Start == Node.PointIndex || IEdge.End == Node.PointIndex) { return; }
				if (Edge.FindSplit(Position, Split))
				{
					Split.NodeIndex = Node.Index;
					InIntersections->Add(EdgeIndex, Split);
				}
			};

			PointsData->GetOctree().FindElementsWithBoundsTest(Edge.Box, ProcessPointRef);
		}
	}

	FEECrossing::FEECrossing(const FEESplit& InSplit)
		: Split(InSplit)
	{
	}

	FEdgeEdgeProxy::FEdgeEdgeProxy(const int32 InEdgeIndex, const FVector& InStart, const FVector& InEnd, const double Tolerance)
	{
		Init(InEdgeIndex, InStart, InEnd, Tolerance);
	}

	void FEdgeEdgeProxy::Init(const int32 InEdgeIndex, const FVector& InStart, const FVector& InEnd, const double Tolerance)
	{
		Intersections.Empty();

		Start = InStart;
		End = InEnd;

		EdgeIndex = InEdgeIndex;
		ToleranceSquared = Tolerance * Tolerance;

		Box = FBox(ForceInit);
		Box += Start;
		Box += End;
		Box = Box.ExpandBy(Tolerance);

		LengthSquared = FVector::DistSquared(Start, End);
		Bounds = Box;

		Direction = (Start - End).GetSafeNormal();
	}

	bool FEdgeEdgeProxy::FindSplit(const FEdgeEdgeProxy& OtherEdge, TArray<FEESplit>& OutSplits) const
	{
		if (!Box.Intersect(OtherEdge.Box) || Start == OtherEdge.Start || Start == OtherEdge.End ||
			End == OtherEdge.End || End == OtherEdge.Start) { return false; }

		// TODO: Check directions/dot

		FVector A;
		FVector B;
		FMath::SegmentDistToSegment(
			Start, End,
			OtherEdge.Start, OtherEdge.End,
			A, B);

		if (FVector::DistSquared(A, B) >= ToleranceSquared) { return false; }

		FEESplit& NewSplit = OutSplits.Emplace_GetRef();
		NewSplit.A = EdgeIndex;
		NewSplit.B = OtherEdge.EdgeIndex;
		NewSplit.Center = FMath::Lerp(A, B, 0.5);
		NewSplit.TimeA = FVector::DistSquared(Start, A) / LengthSquared;
		NewSplit.TimeB = FVector::DistSquared(OtherEdge.Start, B) / OtherEdge.LengthSquared;

		return true;
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

	bool FEdgeEdgeIntersections::AlreadyChecked(const uint64 Key) const
	{
		FReadScopeLock ReadLock(InsertionLock);
		return CheckedPairs.Contains(Key);
	}

	void FEdgeEdgeIntersections::Add_Unsafe(const FEESplit& Split)
	{
		bool bAlreadySet = false;
		CheckedPairs.Add(PCGEx::H64U(Split.A, Split.B), &bAlreadySet);

		// Do not register a crossing that was already found in another test pair
		if (bAlreadySet) { return; }

		const int32 CrossingIndex = Crossings.Num();
		FEECrossing& OutSplit = Crossings.Emplace_GetRef(Split);
		OutSplit.NodeIndex = Graph->Nodes.Num() + CrossingIndex;

		OutSplit.EdgeA = Split.A;
		OutSplit.EdgeB = Split.B;

		// Register crossing index to crossed edges
		Edges[Split.A].Intersections.AddUnique(CrossingIndex);
		Edges[Split.B].Intersections.AddUnique(CrossingIndex);
	}

	void FEdgeEdgeIntersections::BatchAdd(TArray<FEESplit>& Splits, const int32 A)
	{
		FWriteScopeLock WriteLock(InsertionLock);
		for (const FEESplit& Split : Splits) { Add_Unsafe(Split); }
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

	void FindOverlappingEdges(const TSharedRef<FEdgeEdgeIntersections>& InIntersections, const int32 EdgeIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FindOverlappingEdges);

		const FEdgeEdgeProxy& Edge = InIntersections->Edges[EdgeIndex];
		TArray<FEESplit> OutSplits;

		// Find all split points then register crossings that don't exist already

		if (!InIntersections->Details->bEnableSelfIntersection)
		{
			const int32 RootIndex = InIntersections->Graph->FindEdgeMetadata_Unsafe(Edge.EdgeIndex)->RootIndex;
			TSharedPtr<PCGExData::FUnionMetadata> EdgesUnion = InIntersections->Graph->EdgesUnion;
			const TSet<int32>& RootIOIndices = EdgesUnion->Entries[RootIndex]->IOIndices;

			auto ProcessEdge = [&](const FEdgeEdgeProxy* Proxy)
			{
				const FEdgeEdgeProxy& OtherEdge = *Proxy;

				if (OtherEdge.EdgeIndex == -1 || &Edge == &OtherEdge) { return; }
				if (!Edge.Box.Intersect(OtherEdge.Box)) { return; }
				if (InIntersections->Details->bUseMinAngle || InIntersections->Details->bUseMaxAngle)
				{
					if (!InIntersections->Details->CheckDot(FMath::Abs(FVector::DotProduct(Edge.Direction, OtherEdge.Direction)))) { return; }
				}

				// Check overlap last as it's the most expensive op
				if (EdgesUnion->IOIndexOverlap(InIntersections->Graph->FindEdgeMetadata_Unsafe(OtherEdge.EdgeIndex)->RootIndex, RootIOIndices)) { return; }

				if (!Edge.FindSplit(OtherEdge, OutSplits))
				{
				}
			};

			InIntersections->Octree->FindElementsWithBoundsTest(Edge.Box, ProcessEdge);
		}
		else
		{
			auto ProcessEdge = [&](const FEdgeEdgeProxy* Proxy)
			{
				const FEdgeEdgeProxy& OtherEdge = *Proxy;

				if (OtherEdge.EdgeIndex == -1 || &Edge == &OtherEdge) { return; }
				if (!Edge.Box.Intersect(OtherEdge.Box)) { return; }
				if (!Edge.FindSplit(OtherEdge, OutSplits))
				{
				}
			};

			InIntersections->Octree->FindElementsWithBoundsTest(Edge.Box, ProcessEdge);
		}

		// Register crossings
		InIntersections->BatchAdd(OutSplits, EdgeIndex);
	}
}
