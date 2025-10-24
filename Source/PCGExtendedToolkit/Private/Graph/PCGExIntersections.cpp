// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExIntersections.h"

#include "Async/ParallelFor.h"
#include "Details/PCGExDetailsIntersection.h"
#include "PCGExMath.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Geometry/PCGExGeoPointBox.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExEdge.h"
#include "Graph/PCGExGraph.h"
#include "Sampling/PCGExSampling.h"

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
		const TSharedPtr<PCGExData::IUnionData> UnionData = InUnionMetadata->Get(Index);

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

	int32 FUnionGraph::NumNodes() const
	{
		return NodesUnion->Num();
	}

	int32 FUnionGraph::NumEdges() const
	{
		return EdgesUnion->Num();
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

	TSharedPtr<PCGExData::IUnionData> FUnionGraph::InsertEdge(const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To, const PCGExData::FConstPoint& Edge)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(IUnionData::InsertEdge);

		const TSharedPtr<FUnionNode> StartVtx = InsertPoint(From);
		const TSharedPtr<FUnionNode> EndVtx = InsertPoint(To);

		if (StartVtx == EndVtx) { return nullptr; } // Edge got fused entirely

		StartVtx->Add(EndVtx->Index);
		EndVtx->Add(StartVtx->Index);

		TSharedPtr<PCGExData::IUnionData> EdgeUnion;

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

	TSharedPtr<PCGExData::IUnionData> FUnionGraph::InsertEdge_Unsafe(const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To, const PCGExData::FConstPoint& Edge)
	{
		const TSharedPtr<FUnionNode> StartVtx = InsertPoint_Unsafe(From);
		const TSharedPtr<FUnionNode> EndVtx = InsertPoint_Unsafe(To);

		if (StartVtx == EndVtx) { return nullptr; } // Edge got fused entirely

		StartVtx->Adjacency.Add(EndVtx->Index);
		EndVtx->Adjacency.Add(StartVtx->Index);

		const uint64 H = PCGEx::H64U(StartVtx->Index, EndVtx->Index);
		TSharedPtr<PCGExData::IUnionData> EdgeUnion;

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
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionGraph::WriteNodeMetadata)

		InGraph->NodeMetadata.Reserve(Nodes.Num());

		for (const TSharedPtr<FUnionNode>& Node : Nodes)
		{
			const TSharedPtr<PCGExData::IUnionData>& UnionData = NodesUnion->Entries[Node->Index];
			FGraphNodeMetadata& NodeMeta = InGraph->GetOrCreateNodeMetadata_Unsafe(Node->Index);
			NodeMeta.UnionSize = UnionData->Num();
		}
	}

	void FUnionGraph::WriteEdgeMetadata(const TSharedPtr<FGraph>& InGraph) const
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionGraph::WriteEdgeMetadata)

		const int32 NumEdges = Edges.Num();
		InGraph->EdgeMetadata.Reserve(NumEdges);

		for (int i = 0; i < NumEdges; i++)
		{
			const TSharedPtr<PCGExData::IUnionData>& UnionData = EdgesUnion->Entries[i];
			FGraphEdgeMetadata& EdgeMetadata = InGraph->GetOrCreateEdgeMetadata_Unsafe(i);
			EdgeMetadata.UnionSize = UnionData->Num();
		}
		/*
		for (const TPair<uint64, FIndexedEdge>& Pair : Edges)
		{
			const int32 Index = Pair.Value.EdgeIndex;
			const TUniquePtr<PCGExData::IUnionData>& UnionData = EdgesUnion->Items[Index];
			FGraphEdgeMetadata& EdgeMetadata = FGraphEdgeMetadata::GetOrCreate(Index, nullptr, OutMetadata);
			EdgeMetadata.UnionSize = UnionData->Num();
		}
		*/
	}

	void FPointEdgeProxy::Init(const int32 InEdgeIndex, const FVector& InStart, const FVector& InEnd, const double Tolerance)
	{
		CollinearPoints.Empty();

		Start = InStart;
		End = InEnd;

		Index = InEdgeIndex;
		ToleranceSquared = Tolerance * Tolerance;

		Box = PCGEX_BOX_TOLERANCE_INLINE(Start, End, Tolerance);
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

	void FPointEdgeProxy::Add(const FPESplit& Split)
	{
		CollinearPoints.Add(Split);
	}

	bool FPointEdgeProxy::IsEmpty() const
	{
		return !CollinearPoints.IsEmpty();
	}

	FPointEdgeIntersections::FPointEdgeIntersections(
		const TSharedPtr<FGraph>& InGraph,
		const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		const FPCGExPointEdgeIntersectionDetails* InDetails)
		: PointIO(InPointIO), Graph(InGraph), Details(InDetails)
	{
		NodeTransforms = InPointIO->GetOutIn()->GetConstTransformValueRange();
	}

	void FPointEdgeIntersections::Init(const TArray<PCGExMT::FScope>& Loops)
	{
		ScopedEdges = MakeShared<PCGExMT::TScopedArray<TSharedPtr<FPointEdgeProxy>>>(Loops);
	}

	bool FPointEdgeIntersections::InitProxyEdge(const TSharedPtr<FPointEdgeProxy>& Edge, const int32 Index) const
	{
		if (Index == -1) { return false; }

		const FEdge& E = Graph->Edges[Index];

		if (!E.bValid) { return false; }

		Edge->Init(
			Index,
			NodeTransforms[E.Start].GetLocation(),
			NodeTransforms[E.End].GetLocation(),
			Details->FuseDetails.Tolerance);

		return true;
	}

	void FPointEdgeIntersections::Insert()
	{
		// Collapse scoped edges into Edges if initialized
		if (ScopedEdges) { ScopedEdges->Collapse(Edges); }

		Graph->ReserveForEdges(Edges.Num());

		FEdge NewEdge = FEdge{};

		UPCGBasePointData* OutPointData = PointIO->GetOut();
		TPCGValueRange<FTransform> Transforms = OutPointData->GetTransformValueRange(false);

		FGraphEdgeMetadata TempParentMetaCopy = FGraphEdgeMetadata(-1, nullptr); // Required if pointer gets invalidated due to resizing

		// Find how many new metadata needs to be reserved
		int32 EdgeReserve = 0;
		for (const TSharedPtr<FPointEdgeProxy>& PointEdgeProxy : Edges)
		{
			EdgeReserve += PointEdgeProxy->CollinearPoints.Num() + 1;
		}

		Graph->EdgeMetadata.Reserve(Graph->EdgeMetadata.Num() + EdgeReserve);
		Graph->NodeMetadata.Reserve(Graph->NodeMetadata.Num() + EdgeReserve);

		for (const TSharedPtr<FPointEdgeProxy>& PointEdgeProxy : Edges)
		{
			const FEdge& SplitEdge = Graph->Edges[PointEdgeProxy->Index];
			const FGraphEdgeMetadata* ParentEdgeMeta = Graph->FindEdgeMetadata_Unsafe(SplitEdge.Index);

			const bool bValidParent = ParentEdgeMeta != nullptr;
			if (bValidParent) { TempParentMetaCopy = *ParentEdgeMeta; }

			int32 PrevIndex = SplitEdge.Start;
			for (const FPESplit Split : PointEdgeProxy->CollinearPoints)
			{
				const int32 NodeIndex = Split.NodeIndex;

				//TODO: IOIndex required
				if (Graph->InsertEdge(PrevIndex, NodeIndex, NewEdge, SplitEdge.IOIndex))
				{
					FGraphEdgeMetadata* NewEdgeMeta = Graph->AddNodeAndEdgeMetadata_Unsafe(NodeIndex, NewEdge.Index, bValidParent ? &TempParentMetaCopy : nullptr, EPCGExIntersectionType::PointEdge);
					NewEdgeMeta->bIsSubEdge = true;
					if (Details->bSnapOnEdge) { Transforms[Graph->Nodes[Split.NodeIndex].PointIndex].SetLocation(Split.ClosestPoint); }
				}
				else if (FGraphEdgeMetadata* ExistingEdgeMeta = Graph->EdgeMetadata.Find(NewEdge.Index))
				{
					ExistingEdgeMeta->UnionSize++;
					ExistingEdgeMeta->bIsSubEdge = true;
				}


				PrevIndex = NodeIndex;
			}

			// Insert last edge
			if (Graph->InsertEdge(PrevIndex, SplitEdge.End, NewEdge, SplitEdge.IOIndex))
			{
				FGraphEdgeMetadata* NewEdgeMeta = Graph->AddEdgeMetadata_Unsafe(NewEdge.Index, bValidParent ? &TempParentMetaCopy : nullptr, EPCGExIntersectionType::PointEdge);
				NewEdgeMeta->bIsSubEdge = true;
			}
			else if (FGraphEdgeMetadata* ExistingMetadataPtr = Graph->EdgeMetadata.Find(NewEdge.Index))
			{
				ExistingMetadataPtr->UnionSize++;
				ExistingMetadataPtr->bIsSubEdge = true;
			}
		}
	}

	void FPointEdgeIntersections::BlendIntersection(const int32 Index, PCGExDataBlending::FMetadataBlender* Blender) const
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPointEdgeIntersections::BlendIntersection);

		const TSharedPtr<FPointEdgeProxy>& PointEdgeProxy = Edges[Index];

		if (PointEdgeProxy->CollinearPoints.IsEmpty()) { return; }

		const FEdge& SplitEdge = Graph->Edges[PointEdgeProxy->Index];

		const int32 A = SplitEdge.Start;
		const int32 B = SplitEdge.End;

		const TPCGValueRange<FTransform> Transforms = PointIO->GetOut()->GetTransformValueRange(false);

		for (const FPESplit Split : PointEdgeProxy->CollinearPoints)
		{
			const int32 TargetIndex = Graph->Nodes[Split.NodeIndex].PointIndex;
			const FVector& PreBlendLocation = Transforms[TargetIndex].GetLocation();

			Blender->Blend(A, B, TargetIndex, 0.5); // TODO : Compute proper lerp

			Transforms[TargetIndex].SetLocation(PreBlendLocation);
		}
	}

	void FindCollinearNodes(const TSharedPtr<FPointEdgeIntersections>& InIntersections, const TSharedPtr<FPointEdgeProxy>& EdgeProxy)
	{
		const TConstPCGValueRange<FTransform> Transforms = InIntersections->NodeTransforms;
		const TSharedPtr<FGraph> Graph = InIntersections->Graph;

		const FEdge& IEdge = Graph->Edges[EdgeProxy->Index];
		FPESplit Split = FPESplit{};

		InIntersections->PointIO->GetOutIn()->GetPointOctree().FindElementsWithBoundsTest(
			EdgeProxy->Box,
			[&](const PCGPointOctree::FPointRef& PointRef)
			{
				const int32 PointIndex = PointRef.Index;

				if (!Transforms.IsValidIndex(PointIndex)) { return; }
				const FNode& Node = InIntersections->Graph->Nodes[PointIndex];

				if (!Node.bValid) { return; }

				const FVector Position = Transforms[Node.PointIndex].GetLocation();

				if (!EdgeProxy->Box.IsInside(Position)) { return; }
				if (IEdge.Start == Node.PointIndex || IEdge.End == Node.PointIndex) { return; }
				if (EdgeProxy->FindSplit(Position, Split))
				{
					Split.NodeIndex = Node.Index;
					EdgeProxy->Add(Split);
				}
			});
	}

	void FindCollinearNodes_NoSelfIntersections(const TSharedPtr<FPointEdgeIntersections>& InIntersections, const TSharedPtr<FPointEdgeProxy>& EdgeProxy)
	{
		const TConstPCGValueRange<FTransform> Transforms = InIntersections->NodeTransforms;
		const TSharedPtr<FGraph> Graph = InIntersections->Graph;

		const FEdge& IEdge = Graph->Edges[EdgeProxy->Index];
		FPESplit Split = FPESplit{};

		const int32 RootIndex = InIntersections->Graph->FindEdgeMetadata_Unsafe(EdgeProxy->Index)->RootIndex;
		const TSet<int32>& RootIOIndices = Graph->EdgesUnion->Entries[RootIndex]->IOSet;

		InIntersections->PointIO->GetOutIn()->GetPointOctree().FindElementsWithBoundsTest(
			EdgeProxy->Box,
			[&](const PCGPointOctree::FPointRef& PointRef)
			{
				const int32 PointIndex = PointRef.Index;

				if (!Transforms.IsValidIndex(PointIndex)) { return; }
				const FNode& Node = InIntersections->Graph->Nodes[PointIndex];

				if (!Node.bValid) { return; }

				const FVector Position = Transforms[Node.PointIndex].GetLocation();

				if (!EdgeProxy->Box.IsInside(Position)) { return; }
				if (IEdge.Start == Node.PointIndex || IEdge.End == Node.PointIndex) { return; }
				if (!EdgeProxy->FindSplit(Position, Split)) { return; }

				if (Graph->NodesUnion->IOIndexOverlap(Node.Index, RootIOIndices)) { return; }

				Split.NodeIndex = Node.Index;
				EdgeProxy->Add(Split);
			});
	}

	void FEdgeEdgeProxy::Init(const FEdge* InEdge, const FVector& InStart, const FVector& InEnd, const double Tolerance)
	{
		Edge = InEdge;
		Box = PCGEX_BOX_TOLERANCE_INLINE(InStart, InEnd, Tolerance);
	}

	bool FEdgeEdgeProxy::FindSplit(const FEdge& OtherEdge, const TSharedPtr<FEdgeEdgeIntersections>& EEI)
	{
		const FVector Start = EEI->Positions[Edge->Start];
		const FVector End = EEI->Positions[Edge->End];
		const FVector OtherStart = EEI->Positions[OtherEdge.Start];
		const FVector OtherEnd = EEI->Positions[OtherEdge.End];

		FVector A;
		FVector B;
		FMath::SegmentDistToSegment(
			Start, End,
			OtherStart, OtherEnd,
			A, B);

		if (FVector::DistSquared(A, B) >= EEI->Details->ToleranceSquared) { return false; }

		// We're being strict about edge/edge
		if (A == Start || A == End || B == OtherStart || B == OtherEnd) { return false; }

		FEESplit& Split = Crossings.Emplace_GetRef().Split;

		Split.A = Edge->Index;
		Split.B = OtherEdge.Index;
		Split.Center = FMath::Lerp(A, B, 0.5);
		Split.TimeA = FVector::DistSquared(Start, A) / EEI->LengthSquared[Edge->Index];
		Split.TimeB = FVector::DistSquared(OtherStart, B) / EEI->LengthSquared[OtherEdge.Index]; // TODO Compute and cache all edges' square distance?

		return true;
	}

	bool FEdgeEdgeProxy::IsEmpty() const
	{
		return Crossings.IsEmpty();
	}

	FEdgeEdgeIntersections::FEdgeEdgeIntersections(
		const TSharedPtr<FGraph>& InGraph,
		const TSharedPtr<FUnionGraph>& InUnionGraph,
		const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		const FPCGExEdgeEdgeIntersectionDetails* InDetails)
		: PointIO(InPointIO), Graph(InGraph), Details(InDetails)
	{
		NodeTransforms = InPointIO->GetOutIn()->GetConstTransformValueRange();
		Octree = MakeShared<PCGExOctree::FItemOctree>(InUnionGraph->Bounds.GetCenter(), InUnionGraph->Bounds.GetExtent().Length() + (Details->Tolerance * 2));
	}

	void FEdgeEdgeIntersections::Init(const TArray<PCGExMT::FScope>& Loops)
	{
		ScopedEdges = MakeShared<PCGExMT::TScopedArray<TSharedPtr<FEdgeEdgeProxy>>>(Loops);

		const int32 NumEdges = Graph->Edges.Num();
		ValidEdges.Init(false, NumEdges);
		LengthSquared.SetNum(NumEdges);
		Positions.SetNum(Graph->Nodes.Num());

		for (int i = 0; i < Positions.Num(); ++i) { Positions[i] = NodeTransforms[i].GetLocation(); }

		for (const FEdge& Edge : Graph->Edges)
		{
			const FVector A = Positions[Edge.Start];
			const FVector B = Positions[Edge.End];

			const double Len = FVector::DistSquared(A, B);
			if (!Edge.bValid || !FMath::IsNearlyZero(Len)) { continue; }

			const int32 Index = Edge.Index;
			ValidEdges[Index] = true;
			LengthSquared[Index] = Len;
			Directions[Index] = (A - B).GetSafeNormal();

			Octree->AddElement(PCGExOctree::FItem(Index, PCGEX_BOX_TOLERANCE_INLINE(A, B, Details->Tolerance)));
		}
	}

	void FEdgeEdgeIntersections::Collapse()
	{
		// Collapse and dedupe crossings nodes
		// TODO : Need to keep only one of the two existing crossings, and update the duplicate one with its final insertion index

		ScopedEdges->Collapse(Edges);

		const int32 StartIndex = Graph->Nodes.Num();
		TMap<uint64, int32> CrossingIdx; // Edge Index x Edge Index unsigned hash mapped to final crossing index
		// TODO : Reserve CrossingIdx
		int32 Index = -1;

		// TODO : Crossings need to exist only once to be inserted
		// Flush existing crossing as we process them and only keep the (unordered) final node Index
		
		for (const TSharedPtr<FEdgeEdgeProxy>& EdgeProxy : Edges)
		{
			for (FEECrossing& Crossing : EdgeProxy->Crossings)
			{
				const uint64 Key = PCGEx::H64U(EdgeProxy->Edge->Index, Crossing.Split.H64U());
				if (const int32* Existing = CrossingIdx.Find(Key))
				{
					Crossing.NodeIndex = StartIndex + *Existing;
				}
				else
				{
					const int32 NewIndex = ++Index;
					CrossingIdx.Add(Key, NewIndex);
					Crossing.NodeIndex = StartIndex + NewIndex;
					// Copy crossing back to the crossing' pool... UGHHHHH
				}
			}
		}

		// Sort crossings
		ParallelFor(
			Edges.Num(), [&](int32 i)
			{
				const TSharedPtr<FEdgeEdgeProxy>& EdgeProxy = Edges[i];
				const int32 GraphIndex = EdgeProxy->Edge->Index;
				EdgeProxy->Crossings.Sort([&GraphIndex, &EdgeProxy](const FEECrossing& A, const FEECrossing& B) { return A.GetTime(GraphIndex) < B.GetTime(GraphIndex); });
			});
	}

	bool FEdgeEdgeIntersections::InitProxyEdge(const TSharedPtr<FEdgeEdgeProxy>& Edge, const int32 Index) const
	{
		if (Index == -1) { return false; }
		if (!ValidEdges[Index]) { return false; }

		const FEdge& E = Graph->Edges[Index];
		Edge->Init(
			&E,
			Positions[E.Start],
			Positions[E.End],
			Details->Tolerance);

		return true;
	}

	bool FEdgeEdgeIntersections::InsertNodes() const
	{
		if (Crossings.IsEmpty()) { return false; }

		TRACE_CPUPROFILER_EVENT_SCOPE(FEdgeEdgeIntersections::InsertNodes);

		// Insert new nodes
		int32 StartNodeIndex = Graph->Nodes.Num();
		Graph->AddNodes(Crossings.Num(), StartNodeIndex);

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

		// Find how many new metadata needs to be reserved
		int32 EdgeReserve = 0;
		for (TSharedPtr<FEdgeEdgeProxy>& EdgeProxy : Edges) { EdgeReserve += EdgeProxy->Crossings.Num() + 1; }

		Graph->EdgeMetadata.Reserve(Graph->EdgeMetadata.Num() + EdgeReserve);
		Graph->NodeMetadata.Reserve(Graph->NodeMetadata.Num() + EdgeReserve);

		for (TSharedPtr<FEdgeEdgeProxy>& EdgeProxy : Edges)
		{
			const FEdge SplitEdge = Graph->Edges[EdgeProxy->Edge->Index];
			const FGraphEdgeMetadata* ParentEdgeMeta = Graph->FindEdgeMetadata_Unsafe(SplitEdge.Index);

			int32 NodeIndex = -1;
			int32 PrevIndex = SplitEdge.Start;

			for (const FEECrossing& Crossing : EdgeProxy->Crossings)
			{
				NodeIndex = Crossing.NodeIndex;

				//BUG: this is the wrong edge IOIndex

				if (Graph->InsertEdge_Unsafe(PrevIndex, NodeIndex, NewEdge, SplitEdge.IOIndex))
				{
					FGraphEdgeMetadata* NewEdgeMeta = Graph->AddNodeAndEdgeMetadata_Unsafe(NodeIndex, NewEdge.Index, ParentEdgeMeta, EPCGExIntersectionType::EdgeEdge);
					NewEdgeMeta->bIsSubEdge = true;
				}
				else
				{
					// Existing edge
					if (FGraphEdgeMetadata* ExistingEdgeMeta = Graph->EdgeMetadata.Find(NewEdge.Index))
					{
						ExistingEdgeMeta->UnionSize++;
						ExistingEdgeMeta->bIsSubEdge = true;
					}
				}

				PrevIndex = NodeIndex;
			}

			// Insert last edge
			if (Graph->InsertEdge_Unsafe(PrevIndex, SplitEdge.End, NewEdge, SplitEdge.IOIndex))
			{
				FGraphEdgeMetadata* NewEdgeMeta = Graph->AddEdgeMetadata_Unsafe(NewEdge.Index, ParentEdgeMeta, EPCGExIntersectionType::EdgeEdge);
				NewEdgeMeta->bIsSubEdge = true;
			}
			else
			{
				// Existing edge
				if (FGraphEdgeMetadata* ExistingEdgeMeta = Graph->EdgeMetadata.Find(NewEdge.Index))
				{
					ExistingEdgeMeta->UnionSize++;
					ExistingEdgeMeta->bIsSubEdge = true;
				}
			}
		}
	}

	void FEdgeEdgeIntersections::BlendIntersection(const int32 Index, const TSharedRef<PCGExDataBlending::FMetadataBlender>& Blender, TArray<PCGEx::FOpStats>& Trackers) const
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FEdgeEdgeIntersections::BlendIntersection);

		const FEECrossing& Crossing = Crossings[Index];

		const int32 Target = Graph->Nodes[Crossing.NodeIndex].PointIndex;
		Blender->BeginMultiBlend(Target, Trackers);

		const int32 A1 = Graph->Nodes[Graph->Edges[Crossing.Split.A].Start].PointIndex;
		const int32 A2 = Graph->Nodes[Graph->Edges[Crossing.Split.A].End].PointIndex;
		const int32 B1 = Graph->Nodes[Graph->Edges[Crossing.Split.B].Start].PointIndex;
		const int32 B2 = Graph->Nodes[Graph->Edges[Crossing.Split.B].End].PointIndex;

		Blender->MultiBlend(A1, Target, Crossing.Split.TimeA, Trackers);
		Blender->MultiBlend(A2, Target, 1 - Crossing.Split.TimeA, Trackers);
		Blender->MultiBlend(B1, Target, Crossing.Split.TimeB, Trackers);
		Blender->MultiBlend(B2, Target, 1 - Crossing.Split.TimeB, Trackers);

		Blender->EndMultiBlend(Target, Trackers);

		PointIO->GetOutPoint(Target).SetLocation(Crossing.Split.Center);
	}

	void FindOverlappingEdges(
		const TSharedPtr<FEdgeEdgeIntersections>& InIntersections,
		const TSharedPtr<FEdgeEdgeProxy>& EdgeProxy)
	{
		// Find all split points then register crossings that don't exist already
		const int32 GraphIndex = EdgeProxy->Edge->Index;
		const int32 Start = EdgeProxy->Edge->Start;
		const int32 End = EdgeProxy->Edge->End;

		const TArray<FVector>& Directions = InIntersections->Directions;
		InIntersections->Octree->FindElementsWithBoundsTest(
			EdgeProxy->Box,
			[&](const PCGExOctree::FItem& Item)
			{
				const FEdge& OtherEdge = InIntersections->Graph->Edges[Item.Index];
				if (!InIntersections->ValidEdges[Item.Index]
					|| Item.Index == GraphIndex
					|| Start == OtherEdge.Start || Start == OtherEdge.End || End == OtherEdge.End || End == OtherEdge.Start) { return; }

				if (InIntersections->Details->bUseMinAngle || InIntersections->Details->bUseMaxAngle)
				{
					if (!InIntersections->Details->CheckDot(FMath::Abs(FVector::DotProduct(Directions[GraphIndex], Directions[OtherEdge.Index])))) { return; }
				}

				EdgeProxy->FindSplit(OtherEdge, InIntersections);
			});
	}

	void FindOverlappingEdges_NoSelfIntersections(
		const TSharedPtr<FEdgeEdgeIntersections>& InIntersections,
		const TSharedPtr<FEdgeEdgeProxy>& EdgeProxy)
	{
		// Find all split points then register crossings that don't exist already
		const int32 GraphIndex = EdgeProxy->Edge->Index;
		const int32 Start = EdgeProxy->Edge->Start;
		const int32 End = EdgeProxy->Edge->End;

		const TArray<FVector>& Directions = InIntersections->Directions;

		const int32 RootIndex = InIntersections->Graph->FindEdgeMetadata_Unsafe(GraphIndex)->RootIndex;
		TSharedPtr<PCGExData::FUnionMetadata> EdgesUnion = InIntersections->Graph->EdgesUnion;
		const TSet<int32>& RootIOIndices = EdgesUnion->Entries[RootIndex]->IOSet;

		InIntersections->Octree->FindElementsWithBoundsTest(
			EdgeProxy->Box,
			[&](const PCGExOctree::FItem& Item)
			{
				const FEdge& OtherEdge = InIntersections->Graph->Edges[Item.Index];
				if (!InIntersections->ValidEdges[Item.Index]
					|| Item.Index == GraphIndex
					|| Start == OtherEdge.Start || Start == OtherEdge.End || End == OtherEdge.End || End == OtherEdge.Start) { return; }

				if (InIntersections->Details->bUseMinAngle || InIntersections->Details->bUseMaxAngle)
				{
					if (!InIntersections->Details->CheckDot(FMath::Abs(FVector::DotProduct(Directions[GraphIndex], Directions[OtherEdge.Index])))) { return; }
				}

				// Check overlap last as it's the most expensive op
				if (EdgesUnion->IOIndexOverlap(InIntersections->Graph->FindEdgeMetadata_Unsafe(OtherEdge.Index)->RootIndex, RootIOIndices)) { return; }

				EdgeProxy->FindSplit(OtherEdge, InIntersections);
			});
	}
}

FPCGExBoxIntersectionDetails::FPCGExBoxIntersectionDetails()
{
	if (const UEnum* EnumClass = StaticEnum<EPCGExCutType>())
	{
		const int32 NumEnums = EnumClass->NumEnums() - 1; // Skip _MAX 
		for (int32 i = 0; i < NumEnums; ++i) { CutTypeValueMapping.Add(static_cast<EPCGExCutType>(EnumClass->GetValueByIndex(i)), i); }
	}
}

bool FPCGExBoxIntersectionDetails::Validate(const FPCGContext* InContext) const
{
#define PCGEX_LOCAL_DETAIL_CHECK(_NAME, _TYPE, _DEFAULT) if (bWrite##_NAME) { PCGEX_VALIDATE_NAME_C(InContext, _NAME##AttributeName) }
	PCGEX_FOREACH_FIELD_INTERSECTION(PCGEX_LOCAL_DETAIL_CHECK)
#undef PCGEX_LOCAL_DETAIL_CHECK

	return true;
}

void FPCGExBoxIntersectionDetails::Init(const TSharedPtr<PCGExData::FFacade>& PointDataFacade, const TSharedPtr<PCGExSampling::FTargetsHandler>& TargetsHandler)
{
	const int32 NumTargets = TargetsHandler->Num();
	IntersectionForwardHandlers.Init(nullptr, NumTargets);

	TargetsHandler->ForEachTarget(
		[&](const TSharedRef<PCGExData::FFacade>& InTarget, const int32 Index)
		{
			IntersectionForwardHandlers[Index] = IntersectionForwarding.TryGetHandler(InTarget, PointDataFacade, false);
		});

#define PCGEX_LOCAL_DETAIL_WRITER(_NAME, _TYPE, _DEFAULT) if (bWrite##_NAME){ _NAME##Writer = PointDataFacade->GetWritable( _NAME##AttributeName, _DEFAULT, true, PCGExData::EBufferInit::Inherit); }
	PCGEX_FOREACH_FIELD_INTERSECTION(PCGEX_LOCAL_DETAIL_WRITER)
#undef PCGEX_LOCAL_DETAIL_WRITER
}

bool FPCGExBoxIntersectionDetails::WillWriteAny() const
{
#define PCGEX_LOCAL_DETAIL_WILL_WRITE(_NAME, _TYPE, _DEFAULT) if (bWrite##_NAME){ return true; }
	PCGEX_FOREACH_FIELD_INTERSECTION(PCGEX_LOCAL_DETAIL_WILL_WRITE)
#undef PCGEX_LOCAL_DETAIL_WILL_WRITE

	return IntersectionForwarding.bEnabled;
}

void FPCGExBoxIntersectionDetails::Mark(const TSharedRef<PCGExData::FPointIO>& InPointIO) const
{
#define PCGEX_LOCAL_DETAIL_MARK(_NAME, _TYPE, _DEFAULT) if (bWrite##_NAME) { PCGExData::WriteMark(InPointIO, _NAME##AttributeName, _DEFAULT); }
	PCGEX_FOREACH_FIELD_INTERSECTION(PCGEX_LOCAL_DETAIL_MARK)
#undef PCGEX_LOCAL_DETAIL_MARK
}

void FPCGExBoxIntersectionDetails::SetIntersection(const int32 PointIndex, const PCGExGeo::FCut& InCut) const
{
	check(InCut.Idx != -1)
	if (const TSharedPtr<PCGExData::FDataForwardHandler>& IntersectionForwardHandler = IntersectionForwardHandlers[InCut.Idx])
	{
		IntersectionForwardHandler->Forward(InCut.BoxIndex, PointIndex);
	}

	if (IsIntersectionWriter) { IsIntersectionWriter->SetValue(PointIndex, true); }
	if (CutTypeWriter) { CutTypeWriter->SetValue(PointIndex, CutTypeValueMapping[InCut.Type]); }
	if (NormalWriter) { NormalWriter->SetValue(PointIndex, InCut.Normal); }
	if (BoundIndexWriter) { BoundIndexWriter->SetValue(PointIndex, InCut.BoxIndex); }
}
