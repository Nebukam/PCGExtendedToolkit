// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExIntersections.h"

#include "PCGExPointsProcessor.h"


#include "Graph/PCGExCluster.h"

namespace PCGExGraph
{
	FUnionNode::FUnionNode(const PCGExData::FConstPoint& InPoint, const FVector& InCenter, const int32 InIndex)
		: Point(InPoint), Center(InCenter), Index(InIndex)
	{
		Adjacency.Empty();
		Bounds = FBoxSphereBounds(InPoint.Data->GetLocalBounds(InPoint.Index).TransformBy(InPoint.Data->GetTransform(InPoint.Index)));
	}

	FVector FUnionNode::UpdateCenter(const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata, const TSharedPtr<PCGExData::FPointIOCollection>& IOGroup)
	{
		Center = FVector::ZeroVector;
		const TSharedPtr<PCGExData::FUnionData> UnionData = InUnionMetadata->Get(Index);

		const double Divider = UnionData->Elements.Num();

		for (const PCGExData::FElement& H : UnionData->Elements)
		{
			Center += IOGroup->Pairs[H.IO]->GetIn()->GetTransform(H.Index).GetLocation();
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

		NodesUnion = MakeShared<PCGExData::FUnionMetadata>();
		EdgesUnion = MakeShared<PCGExData::FUnionMetadata>();

		if (InFuseDetails.FuseMethod == EPCGExFuseMethod::Octree)
		{
			Octree = MakeUnique<FUnionNodeOctree>(Bounds.GetCenter(), Bounds.GetExtent().Length() + 10);
		}
	}

	bool FUnionGraph::Init(FPCGExContext* InContext)
	{
		return FuseDetails.Init(InContext, nullptr);
	}

	bool FUnionGraph::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InUniqueSourceFacade, const bool SupportScopedGet)
	{
		return FuseDetails.Init(InContext, InUniqueSourceFacade);
	}

	TSharedPtr<FUnionNode> FUnionGraph::InsertPoint(const PCGExData::FConstPoint& Point)
	{
		const FVector Origin = Point.GetLocation();

		TSharedPtr<FUnionNode> Node;

		if (!Octree)
		{
			const uint32 GridKey = FuseDetails.GetGridKey(Origin, Point.Index);
			TSharedPtr<FUnionNode>* NodePtr;

			{
				FReadScopeLock ReadScopeLock(UnionLock);
				NodePtr = GridTree.Find(GridKey);


				if (NodePtr)
				{
					Node = *NodePtr;
					NodesUnion->Append(Node->Index, Point);
					return Node;
				}
			}

			{
				FWriteScopeLock WriteLock(UnionLock);
				NodePtr = GridTree.Find(GridKey); // Make sure there hasn't been an insert while locking

				if (NodePtr)
				{
					Node = *NodePtr;
					NodesUnion->Append(Node->Index, Point);
					return Node;
				}

				Node = MakeShared<FUnionNode>(Point, Origin, Nodes.Num());
				Nodes.Add(Node);
				NodesUnion->NewEntry_Unsafe(Point);
				GridTree.Add(GridKey, Node);
			}

			return Node;
		}

		{
			// Write lock starts
			FWriteScopeLock WriteScopeLock(UnionLock);

			PCGExMath::FClosestPosition ClosestNode(Origin);

			if (FuseDetails.bComponentWiseTolerance)
			{
				Octree->FindElementsWithBoundsTest(
					FuseDetails.GetOctreeBox(Origin, Point.Index), [&](const FUnionNode* ExistingNode)
					{
						if (FuseDetails.IsWithinToleranceComponentWise(Point, ExistingNode->Point))
						{
							ClosestNode.Update(ExistingNode->Center, ExistingNode->Index);
							return false;
						}
						return true;
					});
			}
			else
			{
				Octree->FindElementsWithBoundsTest(
					FuseDetails.GetOctreeBox(Origin, Point.Index), [&](const FUnionNode* ExistingNode)
					{
						if (FuseDetails.IsWithinTolerance(Point, ExistingNode->Point))
						{
							ClosestNode.Update(ExistingNode->Center, ExistingNode->Index);
							return false;
						}
						return true;
					});
			}

			if (ClosestNode.bValid)
			{
				NodesUnion->Append(ClosestNode.Index, Point);
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
			NodesUnion->NewEntry_Unsafe(Point);
		}

		return Node;
	}

	TSharedPtr<FUnionNode> FUnionGraph::InsertPoint_Unsafe(const PCGExData::FConstPoint& Point)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionGraph::InsertPoint_Unsafe);

		const FVector Origin = Point.GetLocation();

		TSharedPtr<FUnionNode> Node;

		if (!Octree)
		{
			const uint32 GridKey = FuseDetails.GetGridKey(Origin, Point.Index);

			if (TSharedPtr<FUnionNode>* NodePtr = GridTree.Find(GridKey))
			{
				Node = *NodePtr;
				NodesUnion->Append(Node->Index, Point);
				return Node;
			}

			Node = MakeShared<FUnionNode>(Point, Origin, Nodes.Num());
			Nodes.Add(Node);
			NodesUnion->NewEntry_Unsafe(Point);
			GridTree.Add(GridKey, Node);

			return Node;
		}

		PCGExMath::FClosestPosition ClosestNode(Origin);

		if (FuseDetails.bComponentWiseTolerance)
		{
			Octree->FindElementsWithBoundsTest(
				FuseDetails.GetOctreeBox(Origin, Point.Index), [&](const FUnionNode* ExistingNode)
				{
					if (FuseDetails.IsWithinToleranceComponentWise(Point, ExistingNode->Point))
					{
						ClosestNode.Update(ExistingNode->Center, ExistingNode->Index);
						return false;
					}
					return true;
				});
		}
		else
		{
			Octree->FindElementsWithBoundsTest(
				FuseDetails.GetOctreeBox(Origin, Point.Index), [&](const FUnionNode* ExistingNode)
				{
					if (FuseDetails.IsWithinTolerance(Point, ExistingNode->Point))
					{
						ClosestNode.Update(ExistingNode->Center, ExistingNode->Index);
						return false;
					}
					return true;
				});
		}

		if (ClosestNode.bValid)
		{
			NodesUnion->Append(ClosestNode.Index, Point);
			return Nodes[ClosestNode.Index];
		}

		Node = MakeShared<FUnionNode>(Point, Origin, Nodes.Num());
		Nodes.Add(Node);
		Octree->AddElement(Node.Get());
		NodesUnion->NewEntry_Unsafe(Point);

		return Node;
	}

	TSharedPtr<PCGExData::FUnionData> FUnionGraph::InsertEdge(const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To, const PCGExData::FConstPoint& Edge)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionData::InsertEdge);

		const TSharedPtr<FUnionNode> StartVtx = InsertPoint(From);
		const TSharedPtr<FUnionNode> EndVtx = InsertPoint(To);

		if (StartVtx == EndVtx) { return nullptr; } // Edge got fused entirely

		StartVtx->Add(EndVtx->Index);
		EndVtx->Add(StartVtx->Index);

		TSharedPtr<PCGExData::FUnionData> EdgeUnion;

		const uint64 H = PCGEx::H64U(StartVtx->Index, EndVtx->Index);

		{
			FReadScopeLock ReadLockEdges(EdgesLock);
			if (const FEdge* ExistingEdge = Edges.Find(H))
			{
				EdgeUnion = EdgesUnion->Entries[ExistingEdge->Index];
			}

			if (EdgeUnion)
			{
				if (Edge.IO == -1)
				{
					// Abstract tracking to get valid union data
					EdgeUnion->Add(PCGExData::FPoint(EdgeUnion->Num(), -1));
				}
				else
				{
					EdgeUnion->Add(Edge);
				}

				return EdgeUnion;
			}
		}

		{
			FWriteScopeLock WriteLockEdges(EdgesLock);

			if (const FEdge* ExistingEdge = Edges.Find(H)) { EdgeUnion = EdgesUnion->Entries[ExistingEdge->Index]; }

			if (EdgeUnion)
			{
				if (Edge.IO == -1)
				{
					// Abstract tracking to get valid union data
					EdgeUnion->Add(PCGExData::FPoint(EdgeUnion->Num(), -1));
				}
				else
				{
					EdgeUnion->Add(Edge);
				}

				return EdgeUnion;
			}

			EdgeUnion = EdgesUnion->NewEntry_Unsafe(Edge);
			Edges.Add(H, FEdge(Edges.Num(), StartVtx->Index, EndVtx->Index));
		}

		return EdgeUnion;
	}

	TSharedPtr<PCGExData::FUnionData> FUnionGraph::InsertEdge_Unsafe(const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To, const PCGExData::FConstPoint& Edge)
	{
		const TSharedPtr<FUnionNode> StartVtx = InsertPoint_Unsafe(From);
		const TSharedPtr<FUnionNode> EndVtx = InsertPoint_Unsafe(To);

		if (StartVtx == EndVtx) { return nullptr; } // Edge got fused entirely

		StartVtx->Adjacency.Add(EndVtx->Index);
		EndVtx->Adjacency.Add(StartVtx->Index);

		const uint64 H = PCGEx::H64U(StartVtx->Index, EndVtx->Index);
		TSharedPtr<PCGExData::FUnionData> EdgeUnion;

		if (Edge.IO == -1)
		{
			// Abstract edge management, so we have some valid metadata even tho there are no valid input edges
			// So EdgeIOIndex will be invalid, be we can still track union data
			if (const FEdge* ExistingEdge = Edges.Find(H))
			{
				EdgeUnion = EdgesUnion->Entries[ExistingEdge->Index];
				EdgeUnion->Add(PCGExData::FPoint(EdgeUnion->Num(), -1));
			}
			else
			{
				// TODO : Need to look into this, apprently we want NewEntry to be force-initialized at ItemIndex 0
				// Which is weird, the Safe version of that same method isn't doing this.
				PCGExData::FConstPoint NewEdge(Edge);
				NewEdge.Index = -1; // Force entry index to use index 0

				EdgeUnion = EdgesUnion->NewEntry_Unsafe(NewEdge);
				Edges.Add(H, FEdge(Edges.Num(), StartVtx->Index, EndVtx->Index));
			}
		}
		else
		{
			// Concrete edge management, we have valild input edges			
			if (const FEdge* ExistingEdge = Edges.Find(H))
			{
				EdgeUnion = EdgesUnion->Entries[ExistingEdge->Index];
				EdgeUnion->Add(Edge);
			}
			else
			{
				EdgeUnion = EdgesUnion->NewEntry_Unsafe(Edge);
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
		const TConstPCGValueRange<FTransform> Transforms = InPointIO->GetOutIn()->GetConstTransformValueRange();

		const int32 NumEdges = InGraph->Edges.Num();
		Edges.SetNum(NumEdges);

		for (const FEdge& Edge : InGraph->Edges)
		{
			if (!Edge.bValid) { continue; }
			Edges[Edge.Index].Init(
				Edge.Index,
				Transforms[Edge.Start].GetLocation(),
				Transforms[Edge.End].GetLocation(),
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

		UPCGBasePointData* OutPointData = PointIO->GetOut();
		TPCGValueRange<FTransform> Transforms = OutPointData->GetTransformValueRange(false);

		for (FPointEdgeProxy& PointEdgeProxy : Edges)
		{
			if (PointEdgeProxy.CollinearPoints.IsEmpty()) { continue; }

			const FEdge& SplitEdge = Graph->Edges[PointEdgeProxy.EdgeIndex];
			const FGraphEdgeMetadata* ParentEdgeMeta = Graph->FindEdgeMetadata_Unsafe(SplitEdge.Index);

			int32 PrevIndex = SplitEdge.Start;
			for (const FPESplit Split : PointEdgeProxy.CollinearPoints)
			{
				const int32 NodeIndex = Split.NodeIndex;

				Graph->InsertEdge(PrevIndex, NodeIndex, NewEdge, SplitEdge.IOIndex); //TODO: IOIndex required
				Graph->AddNodeAndEdgeMetadata_Unsafe(NodeIndex, NewEdge.Index, ParentEdgeMeta, EPCGExIntersectionType::PointEdge);

				PrevIndex = NodeIndex;

				if (Details->bSnapOnEdge)
				{
					Transforms[Graph->Nodes[Split.NodeIndex].PointIndex].SetLocation(Split.ClosestPoint);
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

		const int32 A = SplitEdge.Start;
		const int32 B = SplitEdge.End;

		const TPCGValueRange<FTransform> Transforms = PointIO->GetOut()->GetTransformValueRange(false);

		for (const FPESplit Split : PointEdgeProxy.CollinearPoints)
		{
			const int32 TargetIndex = Graph->Nodes[Split.NodeIndex].PointIndex;
			const FVector& PreBlendLocation = Transforms[TargetIndex].GetLocation();

			Blender->Blend(A, B, TargetIndex, 0.5); // TODO : Compute proper lerp

			Transforms[TargetIndex].SetLocation(PreBlendLocation);
		}
	}

	void FindCollinearNodes(const TSharedPtr<FPointEdgeIntersections>& InIntersections, const int32 EdgeIndex, const UPCGBasePointData* PointsData)
	{
		const TConstPCGValueRange<FTransform> Transforms = PointsData->GetConstTransformValueRange();

		const FPointEdgeProxy& Edge = InIntersections->Edges[EdgeIndex];
		const TSharedPtr<FGraph> Graph = InIntersections->Graph;

		const FEdge& IEdge = Graph->Edges[EdgeIndex];
		FPESplit Split = FPESplit{};

		if (!InIntersections->Details->bEnableSelfIntersection)
		{
			const int32 RootIndex = InIntersections->Graph->FindEdgeMetadata_Unsafe(Edge.EdgeIndex)->RootIndex;
			const TSet<int32>& RootIOIndices = Graph->EdgesUnion->Entries[RootIndex]->IOSet;

			auto ProcessPointRef = [&](const PCGPointOctree::FPointRef& PointRef)
			{
				const int32 PointIndex = PointRef.Index;

				if (!Transforms.IsValidIndex(PointIndex)) { return; }
				const FNode& Node = InIntersections->Graph->Nodes[PointIndex];

				if (!Node.bValid) { return; }

				const FVector Position = Transforms[Node.PointIndex].GetLocation();

				if (!Edge.Box.IsInside(Position)) { return; }
				if (IEdge.Start == Node.PointIndex || IEdge.End == Node.PointIndex) { return; }
				if (!Edge.FindSplit(Position, Split)) { return; }

				if (Graph->NodesUnion->IOIndexOverlap(Node.Index, RootIOIndices)) { return; }

				Split.NodeIndex = Node.Index;
				InIntersections->Add(EdgeIndex, Split);
			};

			PointsData->GetPointOctree().FindElementsWithBoundsTest(Edge.Box, ProcessPointRef);
		}
		else
		{
			auto ProcessPointRef = [&](const PCGPointOctree::FPointRef& PointRef)
			{
				const int32 PointIndex = PointRef.Index;

				if (!Transforms.IsValidIndex(PointIndex)) { return; }
				const FNode& Node = InIntersections->Graph->Nodes[PointIndex];

				if (!Node.bValid) { return; }

				const FVector Position = Transforms[Node.PointIndex].GetLocation();

				if (!Edge.Box.IsInside(Position)) { return; }
				if (IEdge.Start == Node.PointIndex || IEdge.End == Node.PointIndex) { return; }
				if (Edge.FindSplit(Position, Split))
				{
					Split.NodeIndex = Node.Index;
					InIntersections->Add(EdgeIndex, Split);
				}
			};

			PointsData->GetPointOctree().FindElementsWithBoundsTest(Edge.Box, ProcessPointRef);
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
		const TConstPCGValueRange<FTransform> Transforms = InPointIO->GetOutIn()->GetConstTransformValueRange();

		const int32 NumEdges = InGraph->Edges.Num();
		Edges.SetNum(NumEdges);

		Octree = MakeUnique<FEdgeEdgeProxyOctree>(InUnionGraph->Bounds.GetCenter(), InUnionGraph->Bounds.GetExtent().Length() + (Details->Tolerance * 2));

		for (const FEdge& Edge : InGraph->Edges)
		{
			if (!Edge.bValid) { continue; }
			Edges[Edge.Index].Init(
				Edge.Index,
				Transforms[Edge.Start].GetLocation(),
				Transforms[Edge.End].GetLocation(),
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

		TRACE_CPUPROFILER_EVENT_SCOPE(FEdgeEdgeIntersections::InsertNodes);

		// Insert new nodes
		Graph->AddNodes(Crossings.Num());

		UPCGBasePointData* MutablePoints = PointIO->GetOut();
		const int32 StartIndex = MutablePoints->GetNumPoints();

		MutablePoints->SetNumPoints(Graph->Nodes.Num());
		const int32 NumPoints = MutablePoints->GetNumPoints();

		UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;

		TPCGValueRange<int64> MetadataEntries = PointIO->GetOut()->GetMetadataEntryValueRange(false);
		for (int i = StartIndex; i < NumPoints; i++) { Metadata->InitializeOnSet(MetadataEntries[i]); }

		return true;
	}

	void FEdgeEdgeIntersections::InsertEdges()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FEdgeEdgeIntersections::InsertEdges);

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

	void FEdgeEdgeIntersections::BlendIntersection(const int32 Index, const TSharedRef<PCGExDataBlending::FMetadataBlender>& Blender, TArray<PCGEx::FOpStats>& Trackers) const
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FEdgeEdgeIntersections::BlendIntersection);

		const FEECrossing& Crossing = Crossings[Index];

		const int32 Target = Graph->Nodes[Crossing.NodeIndex].PointIndex;
		Blender->BeginMultiBlend(Target, Trackers);

		const int32 A1 = Graph->Nodes[Graph->Edges[Crossing.EdgeA].Start].PointIndex;
		const int32 A2 = Graph->Nodes[Graph->Edges[Crossing.EdgeA].End].PointIndex;
		const int32 B1 = Graph->Nodes[Graph->Edges[Crossing.EdgeB].Start].PointIndex;
		const int32 B2 = Graph->Nodes[Graph->Edges[Crossing.EdgeB].End].PointIndex;

		Blender->MultiBlend(A1, Target, Crossing.Split.TimeA, Trackers);
		Blender->MultiBlend(A2, Target, 1 - Crossing.Split.TimeA, Trackers);
		Blender->MultiBlend(B1, Target, Crossing.Split.TimeB, Trackers);
		Blender->MultiBlend(B2, Target, 1 - Crossing.Split.TimeB, Trackers);

		Blender->EndMultiBlend(Target, Trackers);

		PointIO->GetOutPoint(Target).SetLocation(Crossing.Split.Center);
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
			const TSet<int32>& RootIOIndices = EdgesUnion->Entries[RootIndex]->IOSet;

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
