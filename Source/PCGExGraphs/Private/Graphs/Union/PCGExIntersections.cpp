// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graphs/Union/PCGExIntersections.h"

#include "Async/ParallelFor.h"
#include "Details/PCGExIntersectionDetails.h"
#include "Data/PCGExPointIO.h"
#include "Blenders/PCGExMetadataBlender.h"
#include "Clusters/PCGExCluster.h"
#include "Clusters/PCGExEdge.h"
#include "Data/PCGExData.h"
#include "Core/PCGExUnionData.h"
#include "Data/Utils/PCGExDataForward.h"
#include "Graphs/PCGExGraph.h"
#include "Graphs/PCGExGraphMetadata.h"
#include "Math/PCGExMath.h"

namespace PCGExGraphs
{
#pragma region FUnionNode

	FUnionNode::FUnionNode(const PCGExData::FConstPoint& InPoint, const FVector& InCenter, const int32 InIndex)
		: Point(InPoint), Center(InCenter), Index(InIndex), bFinalized(false)
	{
		Bounds = FBoxSphereBounds(
			InPoint.Data->GetLocalBounds(InPoint.Index)
			       .TransformBy(InPoint.Data->GetTransform(InPoint.Index)));

		// Reserve initial capacity (doesn't allocate until first use, just sets Max)
		AdjacencyBuffer.Reserve(8);
	}

	FVector FUnionNode::UpdateCenter(
		const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata,
		const TSharedPtr<PCGExData::FPointIOCollection>& IOGroup)
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
		if (bFinalized)
		{
			// Already finalized - direct add to set (with lock for safety)
			FScopeLock Lock(&GrowthLock);
			Adjacency.Add(InAdjacency);
			return;
		}

		// Atomic increment to claim our slot
		const int32 Slot = AdjacencyWriteIndex.fetch_add(1, std::memory_order_relaxed);

		// Ensure buffer has space (lock only when growing)
		{
			FScopeLock Lock(&GrowthLock);
			if (Slot >= AdjacencyBuffer.Num())
			{
				// Grow to accommodate this slot plus some extra
				const int32 NewSize = FMath::Max(Slot + 1, AdjacencyBuffer.Num() * 2);
				const int32 MinSize = FMath::Max(NewSize, 8);
				AdjacencyBuffer.SetNumUninitialized(MinSize, EAllowShrinking::No);
			}
		}

		AdjacencyBuffer[Slot] = InAdjacency;
	}

	void FUnionNode::Finalize()
	{
		if (bFinalized) { return; }

		const int32 NumEntries = AdjacencyWriteIndex.load(std::memory_order_acquire);

		if (NumEntries > 0)
		{
			Adjacency.Reserve(NumEntries);

			for (int32 i = 0; i < NumEntries; ++i)
			{
				Adjacency.Add(AdjacencyBuffer[i]);
			}
		}

		// Release buffer memory
		AdjacencyBuffer.Empty();
		bFinalized = true;
	}

#pragma endregion

#pragma region FUnionGraph

	FUnionGraph::FUnionGraph(
		const FPCGExFuseDetails& InFuseDetails,
		const FBox& InBounds,
		const TSharedPtr<PCGExData::FPointIOCollection>& InSourceCollection)
		: SourceCollection(InSourceCollection)
		  , FuseDetails(InFuseDetails)
		  , Bounds(InBounds)
	{
		Nodes.Empty();
		Edges.Empty();

		NodesUnion = MakeShared<PCGExData::FUnionMetadata>();
		EdgesUnion = MakeShared<PCGExData::FUnionMetadata>();

		if (FuseDetails.FuseMethod == EPCGExFuseMethod::Octree)
		{
			Octree = MakeUnique<FUnionNodeOctree>(
				Bounds.GetCenter(),
				Bounds.GetExtent().Length() + 10);
		}
	}

	bool FUnionGraph::Init(FPCGExContext* InContext)
	{
		return FuseDetails.Init(InContext, nullptr);
	}

	bool FUnionGraph::Init(
		FPCGExContext* InContext,
		const TSharedPtr<PCGExData::FFacade>& InUniqueSourceFacade,
		const bool SupportScopedGet)
	{
		return FuseDetails.Init(InContext, InUniqueSourceFacade);
	}

	void FUnionGraph::Reserve(const int32 NodeReserve, const int32 EdgeReserve)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionGraph::Reserve);

		if (!Octree)
		{
			if (FuseDetails.DoInlineInsertion()) { NodeBins.Reserve(NodeReserve); }
			else { NodeBinsShards.Reserve(NodeReserve); }
		}

		Nodes.Reserve(NodeReserve);
		NodesUnion->Entries.Reserve(NodeReserve);

		const int32 EffectiveEdgeReserve = EdgeReserve < 0 ? NodeReserve : EdgeReserve;

		if (FuseDetails.DoInlineInsertion()) { EdgesMap.Reserve(EffectiveEdgeReserve); }
		else { EdgesMapShards.Reserve(EffectiveEdgeReserve); }

		Edges.Reserve(EffectiveEdgeReserve);
		EdgesUnion->Entries.Reserve(EffectiveEdgeReserve);
	}

	void FUnionGraph::BeginConcurrentBuild(const int32 NodeReserve, const int32 EdgeReserve)
	{
		NodesUnion->BeginConcurrentBuild(NodeReserve);
		EdgesUnion->BeginConcurrentBuild(EdgeReserve < 0 ? NodeReserve : EdgeReserve);
	}

	void FUnionGraph::EndConcurrentBuild()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionGraph::EndConcurrentBuild);

		// Finalize union metadata (merges thread-local buffers)
		NodesUnion->EndConcurrentBuild();
		EdgesUnion->EndConcurrentBuild();

		// Finalize all node adjacencies in parallel
		const int32 NumNodes = Nodes.Num();
		if (NumNodes > 0)
		{
			ParallelFor(NumNodes, [this](const int32 i)
			{
				Nodes[i]->Finalize();
			});
		}

		// Collapse edge tracking structures
		Collapse();
	}

	int32 FUnionGraph::InsertPoint(const PCGExData::FConstPoint& Point)
	{
		const FVector Origin = Point.GetLocation();

		if (!Octree)
		{
			const uint64 GridKey = FuseDetails.GetGridKey(Origin, Point.Index);

			// Fast path: check sharded map first (uses internal per-shard locks)
			if (const int32* NodePtr = NodeBinsShards.Find(GridKey))
			{
				const int32 NodeIndex = *NodePtr;
				NodesUnion->Append(NodeIndex, Point);
				return NodeIndex;
			}

			// Slow path: need to potentially create new entry
			FWriteScopeLock WriteLock(UnionLock);

			// Double-check after acquiring lock
			if (const int32* NodePtr = NodeBinsShards.Find(GridKey))
			{
				const int32 NodeIndex = *NodePtr;
				NodesUnion->Append(NodeIndex, Point);
				return NodeIndex;
			}

			// Create new entry
			const int32 NewIndex = NodesUnion->NewEntry(Point);
			NodeBinsShards.Add(GridKey, Nodes.Add(MakeShared<FUnionNode>(Point, Origin, NewIndex)));
			return NewIndex;
		}

		// Octree-based fusing
		{
			FWriteScopeLock WriteScopeLock(UnionLock);
			PCGExMath::FClosestPosition ClosestNode(Origin);

			Octree->FindElementsWithBoundsTest(
				FuseDetails.GetOctreeBox(Origin, Point.Index),
				[&](const FUnionNode* ExistingNode)
				{
					const bool bIsWithin = FuseDetails.bComponentWiseTolerance
						                       ? FuseDetails.IsWithinToleranceComponentWise(Point, ExistingNode->Point)
						                       : FuseDetails.IsWithinTolerance(Point, ExistingNode->Point);

					if (bIsWithin)
					{
						ClosestNode.Update(ExistingNode->Center, ExistingNode->Index);
						return false;
					}
					return true;
				});

			if (ClosestNode.bValid)
			{
				NodesUnion->Append(ClosestNode.Index, Point);
				return ClosestNode.Index;
			}

			// Create new node while still holding lock
			const int32 NewIndex = NodesUnion->NewEntry(Point);
			const TSharedPtr<FUnionNode> Node = MakeShared<FUnionNode>(Point, Origin, NewIndex);
			Octree->AddElement(Node.Get());
			return Nodes.Add(Node);
		}
	}

	int32 FUnionGraph::InsertPoint_Unsafe(const PCGExData::FConstPoint& Point)
	{
		const FVector Origin = Point.GetLocation();

		if (!Octree)
		{
			const uint64 GridKey = FuseDetails.GetGridKey(Origin, Point.Index);

			if (const int32* NodePtr = NodeBins.Find(GridKey))
			{
				const int32 NodeIndex = *NodePtr;
				NodesUnion->Append_Unsafe(NodeIndex, Point);
				return NodeIndex;
			}

			const int32 NewIndex = NodesUnion->NewEntry_Unsafe(Point);
			return NodeBins.Add(GridKey, Nodes.Add(MakeShared<FUnionNode>(Point, Origin, NewIndex)));
		}

		PCGExMath::FClosestPosition ClosestNode(Origin);

		Octree->FindElementsWithBoundsTest(
			FuseDetails.GetOctreeBox(Origin, Point.Index),
			[&](const FUnionNode* ExistingNode)
			{
				const bool bIsWithin = FuseDetails.bComponentWiseTolerance
					                       ? FuseDetails.IsWithinToleranceComponentWise(Point, ExistingNode->Point)
					                       : FuseDetails.IsWithinTolerance(Point, ExistingNode->Point);

				if (bIsWithin)
				{
					ClosestNode.Update(ExistingNode->Center, ExistingNode->Index);
					return false;
				}
				return true;
			});

		if (ClosestNode.bValid)
		{
			NodesUnion->Append_Unsafe(ClosestNode.Index, Point);
			return ClosestNode.Index;
		}

		const int32 NewIndex = NodesUnion->NewEntry_Unsafe(Point);
		const TSharedPtr<FUnionNode> Node = MakeShared<FUnionNode>(Point, Origin, NewIndex);
		Octree->AddElement(Node.Get());
		return Nodes.Add(Node);
	}

	void FUnionGraph::InsertEdge(
		const PCGExData::FConstPoint& From,
		const PCGExData::FConstPoint& To,
		const PCGExData::FConstPoint& Edge)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionGraph::InsertEdge);

		const int32 Start = InsertPoint(From);
		const int32 End = InsertPoint(To);

		if (Start == End) { return; } // Edge got fused entirely

		const TSharedPtr<FUnionNode> StartVtx = Nodes[Start];
		const TSharedPtr<FUnionNode> EndVtx = Nodes[End];

		// Lock-free adjacency recording via atomic buffer
		StartVtx->Add(End);
		EndVtx->Add(Start);

		const uint64 H = PCGEx::H64U(Start, End);

		// Lambda for updating existing edge union
		auto UpdateExistingUnion = [&](const int32* ExistingEdge)
		{
			TSharedPtr<PCGExData::IUnionData> EdgeUnion = EdgesUnion->Entries[*ExistingEdge];
			if (Edge.IO == -1)
			{
				// Abstract tracking
				EdgeUnion->Add(EdgeUnion->Num(), -1);
			}
			else
			{
				EdgesUnion->Append(*ExistingEdge, Edge);
			}
		};

		// Fast path: check if edge already exists
		if (const int32* ExistingEdge = EdgesMapShards.Find(H))
		{
			UpdateExistingUnion(ExistingEdge);
			return;
		}

		// Slow path: need to potentially create new edge
		{
			FWriteScopeLock WriteLockEdges(EdgesLock);

			// Double-check after lock
			if (const int32* ExistingEdge = EdgesMapShards.Find(H))
			{
				UpdateExistingUnion(ExistingEdge);
				return;
			}

			// Create new edge
			EdgesUnion->NewEntry(Edge);
			EdgesMapShards.Add(H, Edges.Emplace(Edges.Num(), Start, End));
		}
	}

	void FUnionGraph::InsertEdge_Unsafe(
		const PCGExData::FConstPoint& From,
		const PCGExData::FConstPoint& To,
		const PCGExData::FConstPoint& Edge)
	{
		const int32 Start = InsertPoint_Unsafe(From);
		const int32 End = InsertPoint_Unsafe(To);

		if (Start == End) { return; }

		const TSharedPtr<FUnionNode> StartVtx = Nodes[Start];
		const TSharedPtr<FUnionNode> EndVtx = Nodes[End];

		// In unsafe mode, still use Add() - buffer handles it fine
		StartVtx->Add(End);
		EndVtx->Add(Start);

		const uint64 H = PCGEx::H64U(Start, End);

		if (Edge.IO == -1)
		{
			// Abstract edge management
			if (const int32* ExistingEdge = EdgesMap.Find(H))
			{
				TSharedPtr<PCGExData::IUnionData> EdgeUnion = EdgesUnion->Entries[*ExistingEdge];
				EdgeUnion->Add(EdgeUnion->Num(), -1);
			}
			else
			{
				PCGExData::FConstPoint NewEdge(Edge);
				NewEdge.Index = -1; // Force entry index to use index 0
				EdgesUnion->NewEntry_Unsafe(NewEdge);
				EdgesMap.Add(H, Edges.Emplace(Edges.Num(), Start, End));
			}
		}
		else
		{
			// Concrete edge management
			if (const int32* ExistingEdge = EdgesMap.Find(H))
			{
				EdgesUnion->Entries[*ExistingEdge]->Add(Edge);
			}
			else
			{
				EdgesUnion->NewEntry_Unsafe(Edge);
				EdgesMap.Add(H, Edges.Emplace(Edges.Num(), Start, End));
			}
		}
	}

	void FUnionGraph::GetUniqueEdges(TArray<FEdge>& OutEdges)
	{
		OutEdges.Reserve(Edges.Num());
		OutEdges.Append(Edges);
		Edges.Empty();
	}

	void FUnionGraph::WriteNodeMetadata(const TSharedPtr<FGraph>& InGraph) const
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionGraph::WriteNodeMetadata);

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
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionGraph::WriteEdgeMetadata);

		const int32 NumEdges = GetNumCollapsedEdges();
		InGraph->EdgeMetadata.Reserve(NumEdges);

		for (int i = 0; i < NumEdges; i++)
		{
			const TSharedPtr<PCGExData::IUnionData>& UnionData = EdgesUnion->Entries[i];
			FGraphEdgeMetadata& EdgeMetadata = InGraph->GetOrCreateEdgeMetadata_Unsafe(i);
			EdgeMetadata.UnionSize = UnionData->Num();
		}
	}

	void FUnionGraph::Collapse()
	{
		NumCollapsedEdges = Edges.Num();
		EdgesMapShards.Empty();
		EdgesMap.Empty();
	}

#pragma endregion

#pragma region FIntersectionCache

	FIntersectionCache::FIntersectionCache(
		const TSharedPtr<FGraph>& InGraph,
		const TSharedPtr<PCGExData::FPointIO>& InPointIO)
		: PointIO(InPointIO)
		  , Graph(InGraph)
	{
		NodeTransforms = InPointIO->GetOutIn()->GetConstTransformValueRange();
	}

	bool FIntersectionCache::InitProxy(const TSharedPtr<FEdgeProxy>& Edge, const int32 Index) const
	{
		if (Index == -1) { return false; }

		const FEdge* E = &Graph->Edges[Index];
		if (!E->bValid) { return false; }

		Edge->Init(*E, Positions[E->Start], Positions[E->End], Tolerance);
		return true;
	}

	void FIntersectionCache::BuildCache()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FIntersectionCache::BuildCache);

		const int32 NumEdges = Graph->Edges.Num();

		ValidEdges.Init(false, NumEdges);
		LengthSquared.SetNum(NumEdges);
		Directions.SetNum(NumEdges);
		Positions.SetNum(Graph->Nodes.Num());

		for (int i = 0; i < Positions.Num(); ++i) { Positions[i] = NodeTransforms[i].GetLocation(); }

		for (const FEdge& Edge : Graph->Edges)
		{
			const FVector A = Positions[Edge.Start];
			const FVector B = Positions[Edge.End];

			const double Len = FVector::DistSquared(A, B);
			if (!Edge.bValid || FMath::IsNearlyZero(Len)) { continue; }

			const int32 Index = Edge.Index;
			ValidEdges[Index] = true;
			LengthSquared[Index] = Len;
			Directions[Index] = (A - B).GetSafeNormal();

			if (Octree) { Octree->AddElement(PCGExOctree::FItem(Index, PCGEX_BOX_TOLERANCE_INLINE(A, B, Tolerance))); }
		}
	}

#pragma endregion

#pragma region FEdgeProxy

	void FEdgeProxy::Init(
		const FEdge& InEdge,
		const FVector& InStart,
		const FVector& InEnd,
		const double InTolerance)
	{
		Index = InEdge.Index;
		Start = InEdge.Start;
		End = InEdge.End;
		Box = FBox(ForceInit);
		Box += InStart;
		Box += InEnd;
		Box = Box.ExpandBy(InTolerance);
	}

#pragma endregion

#pragma region Point-Edge Intersections

	void FPointEdgeProxy::Init(
		const FEdge& InEdge,
		const FVector& InStart,
		const FVector& InEnd,
		const double InTolerance)
	{
		FEdgeProxy::Init(InEdge, InStart, InEnd, InTolerance);
		CollinearPoints.Empty();
	}

	bool FPointEdgeProxy::FindSplit(
		const int32 PointIndex,
		const TSharedPtr<FIntersectionCache>& Cache,
		FPESplit& OutSplit) const
	{
		const FVector& Position = Cache->Positions[PointIndex];
		const FVector& StartPos = Cache->Positions[Start];
		const FVector& EndPos = Cache->Positions[End];

		const FVector ClosestPoint = FMath::ClosestPointOnSegment(Position, StartPos, EndPos);
		const double DistSq = FVector::DistSquared(Position, ClosestPoint);

		if (DistSq > Cache->ToleranceSquared) { return false; }

		OutSplit.ClosestPoint = ClosestPoint;
		OutSplit.Time = FVector::DistSquared(StartPos, ClosestPoint) / Cache->LengthSquared[Index];

		return true;
	}

	void FPointEdgeProxy::Add(const FPESplit& Split)
	{
		CollinearPoints.Add(Split);
	}

	FPointEdgeIntersections::FPointEdgeIntersections(
		const TSharedPtr<FGraph>& InGraph,
		const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		const FPCGExPointEdgeIntersectionDetails* InDetails)
		: FIntersectionCache(InGraph, InPointIO)
		  , Details(InDetails)
	{
		Tolerance = Details->FuseDetails.Tolerance;
		ToleranceSquared = FMath::Square(Tolerance);
	}

	void FPointEdgeIntersections::Init(const TArray<PCGExMT::FScope>& Loops)
	{
		ScopedEdges = MakeShared<PCGExMT::TScopedArray<TSharedPtr<FPointEdgeProxy>>>(Loops);
		BuildCache();
	}

	void FPointEdgeIntersections::InsertEdges()
	{
		// Collapse scoped edges into Edges if initialized
		if (ScopedEdges) { ScopedEdges->Collapse(Edges); }

		FEdge NewEdge = FEdge{};

		UPCGBasePointData* OutPointData = PointIO->GetOut();
		TPCGValueRange<FTransform> Transforms = OutPointData->GetTransformValueRange(false);

		// Find how many new metadata needs to be reserved
		int32 EdgeReserve = 0;
		for (const TSharedPtr<FPointEdgeProxy>& PointEdgeProxy : Edges) { EdgeReserve += PointEdgeProxy->CollinearPoints.Num() + 1; }

		Graph->ReserveForEdges(EdgeReserve, true);

		for (const TSharedPtr<FPointEdgeProxy>& EdgeProxy : Edges)
		{
			const int32 IOIndex = Graph->Edges[EdgeProxy->Index].IOIndex;
			const int32 RootIndex = Graph->FindEdgeMetadataRootIndex_Unsafe(EdgeProxy->Index);

			int32 A = EdgeProxy->Start;
			for (const FPESplit Split : EdgeProxy->CollinearPoints)
			{
				const int32 B = Split.Index;

				//TODO: IOIndex required
				if (Graph->InsertEdge_Unsafe(A, B, NewEdge, IOIndex))
				{
					FGraphEdgeMetadata& NewEdgeMeta = Graph->AddNodeAndEdgeMetadata_Unsafe(B, NewEdge.Index, RootIndex, EPCGExIntersectionType::PointEdge);
					NewEdgeMeta.bIsSubEdge = true;
					if (Details->bSnapOnEdge) { Transforms[Graph->Nodes[Split.Index].PointIndex].SetLocation(Split.ClosestPoint); }
				}
				else if (FGraphEdgeMetadata* ExistingEdgeMeta = Graph->EdgeMetadata.Find(NewEdge.Index))
				{
					ExistingEdgeMeta->UnionSize++;
					ExistingEdgeMeta->bIsSubEdge = true;
				}

				A = B;
			}

			// Insert last edge
			if (Graph->InsertEdge_Unsafe(A, EdgeProxy->End, NewEdge, IOIndex))
			{
				FGraphEdgeMetadata& NewEdgeMeta = Graph->AddEdgeMetadata_Unsafe(NewEdge.Index, RootIndex, EPCGExIntersectionType::PointEdge);
				NewEdgeMeta.bIsSubEdge = true;
			}
			else if (FGraphEdgeMetadata* ExistingMetadataPtr = Graph->EdgeMetadata.Find(NewEdge.Index))
			{
				ExistingMetadataPtr->UnionSize++;
				ExistingMetadataPtr->bIsSubEdge = true;
			}
		}
	}

	void FPointEdgeIntersections::BlendIntersection(
		const int32 Index,
		PCGExBlending::FMetadataBlender* Blender) const
	{
		const TSharedPtr<FPointEdgeProxy>& PointEdgeProxy = Edges[Index];

		if (PointEdgeProxy->CollinearPoints.IsEmpty()) { return; }

		const FEdge& SplitEdge = Graph->Edges[PointEdgeProxy->Index];

		const int32 A = SplitEdge.Start;
		const int32 B = SplitEdge.End;

		const TPCGValueRange<FTransform> Transforms = PointIO->GetOut()->GetTransformValueRange(false);

		for (const FPESplit Split : PointEdgeProxy->CollinearPoints)
		{
			const int32 TargetIndex = Graph->Nodes[Split.Index].PointIndex;
			const FVector& PreBlendLocation = Transforms[TargetIndex].GetLocation();

			Blender->Blend(A, B, TargetIndex, 0.5); // TODO : Compute proper lerp

			Transforms[TargetIndex].SetLocation(PreBlendLocation);
		}
	}

	void FindCollinearNodes(
		const TSharedPtr<FPointEdgeIntersections>& InIntersections,
		const TSharedPtr<FPointEdgeProxy>& EdgeProxy)
	{
		const TConstPCGValueRange<FTransform> Transforms = InIntersections->NodeTransforms;
		const TSharedPtr<FGraph> Graph = InIntersections->Graph;

		const FEdge& IEdge = Graph->Edges[EdgeProxy->Index];
		FPESplit Split = FPESplit{};

		InIntersections->PointIO->GetOutIn()->GetPointOctree().FindElementsWithBoundsTest(EdgeProxy->Box, [&](const PCGPointOctree::FPointRef& PointRef)
		{
			const int32 PointIndex = PointRef.Index;

			if (!Transforms.IsValidIndex(PointIndex)) { return; }
			const FNode& Node = InIntersections->Graph->Nodes[PointIndex];

			if (!Node.bValid) { return; }

			const FVector Position = Transforms[Node.PointIndex].GetLocation();

			if (!EdgeProxy->Box.IsInside(Position)) { return; }
			if (IEdge.Contains(Node.PointIndex)) { return; }
			if (EdgeProxy->FindSplit(Node.PointIndex, InIntersections, Split))
			{
				Split.Index = Node.Index;
				EdgeProxy->Add(Split);
			}
		});
	}

	void FindCollinearNodes_NoSelfIntersections(
		const TSharedPtr<FPointEdgeIntersections>& InIntersections,
		const TSharedPtr<FPointEdgeProxy>& EdgeProxy)
	{
		const TConstPCGValueRange<FTransform> Transforms = InIntersections->NodeTransforms;
		const TSharedPtr<FGraph> Graph = InIntersections->Graph;

		const FEdge& IEdge = Graph->Edges[EdgeProxy->Index];
		FPESplit Split = FPESplit{};

		const int32 EdgeRootIndex = InIntersections->Graph->FindEdgeMetadataRootIndex_Unsafe(EdgeProxy->Index);
		const TSet<int32>& RootIOIndices = Graph->EdgesUnion->Entries[EdgeRootIndex]->IOSet;

		InIntersections->PointIO->GetOutIn()->GetPointOctree().FindElementsWithBoundsTest(EdgeProxy->Box, [&](const PCGPointOctree::FPointRef& PointRef)
		{
			const int32 PointIndex = PointRef.Index;

			if (!Transforms.IsValidIndex(PointIndex)) { return; }
			const FNode& Node = InIntersections->Graph->Nodes[PointIndex];

			if (!Node.bValid) { return; }

			const FVector Position = Transforms[Node.PointIndex].GetLocation();

			if (!EdgeProxy->Box.IsInside(Position)) { return; }
			if (IEdge.Contains(Node.PointIndex)) { return; }
			if (!EdgeProxy->FindSplit(Node.PointIndex, InIntersections, Split)) { return; }

			if (Graph->NodesUnion->IOIndexOverlap(Node.Index, RootIOIndices)) { return; }

			Split.Index = Node.Index;
			EdgeProxy->Add(Split);
		});
	}

#pragma endregion

#pragma region Edge-Edge Intersections

	bool FEdgeEdgeProxy::FindSplit(
		const FEdge& OtherEdge,
		const TSharedPtr<FIntersectionCache>& Cache)
	{
		const FVector A0 = Cache->Positions[Start];
		const FVector B0 = Cache->Positions[End];
		const FVector A1 = Cache->Positions[OtherEdge.Start];
		const FVector B1 = Cache->Positions[OtherEdge.End];

		FVector A;
		FVector B;
		FMath::SegmentDistToSegment(A0, B0, A1, B1, A, B);

		if (FVector::DistSquared(A, B) >= Cache->ToleranceSquared) { return false; }

		// Strict about edge/edge - endpoints don't count
		if (A == A0 || A == B0 || B == A1 || B == B1) { return false; }

		FEESplit& Split = Crossings.Emplace_GetRef().Split;

		Split.A = Index;
		Split.B = OtherEdge.Index;
		Split.Center = FMath::Lerp(A, B, 0.5);
		Split.TimeA = FVector::DistSquared(A0, A) / Cache->LengthSquared[Index];
		Split.TimeB = FVector::DistSquared(A1, B) / Cache->LengthSquared[OtherEdge.Index];

		return true;
	}

	FEdgeEdgeIntersections::FEdgeEdgeIntersections(
		const TSharedPtr<FGraph>& InGraph,
		const TSharedPtr<FUnionGraph>& InUnionGraph,
		const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		const FPCGExEdgeEdgeIntersectionDetails* InDetails)
		: FIntersectionCache(InGraph, InPointIO)
		  , Details(InDetails)
	{
		Tolerance = Details->Tolerance;
		ToleranceSquared = Details->ToleranceSquared;
		Octree = MakeShared<PCGExOctree::FItemOctree>(
			InUnionGraph->Bounds.GetCenter(),
			InUnionGraph->Bounds.GetExtent().Length() + (Details->Tolerance * 2));
	}

	void FEdgeEdgeIntersections::Init(const TArray<PCGExMT::FScope>& Loops)
	{
		ScopedEdges = MakeShared<PCGExMT::TScopedArray<TSharedPtr<FEdgeEdgeProxy>>>(Loops);
		BuildCache();
	}

	void FEdgeEdgeIntersections::Collapse(const int32 InReserve)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FEdgeEdgeIntersections::Collapse);

		ScopedEdges->Collapse(Edges);

		if (Edges.IsEmpty()) { return; }

		TSet<uint64> SeenCrossings;
		SeenCrossings.Reserve(InReserve);
		UniqueCrossings.Reserve(InReserve);

		for (const TSharedPtr<FEdgeEdgeProxy>& Edge : Edges)
		{
			for (FEECrossing& Crossing : Edge->Crossings)
			{
				bool bAlreadySet = false;
				SeenCrossings.Add(Crossing.Split.H64U(), &bAlreadySet);
				if (!bAlreadySet)
				{
					Crossing.Index = UniqueCrossings.Num();
					UniqueCrossings.Add(Crossing);
				}
			}
		}
	}

	bool FEdgeEdgeIntersections::InsertNodes(const int32 InReserve)
	{
		Collapse(InReserve);

		if (UniqueCrossings.IsEmpty()) { return false; }

		TRACE_CPUPROFILER_EVENT_SCOPE(FEdgeEdgeIntersections::InsertNodes);

		// Insert new nodes
		int32 StartIndex = -1;
		Graph->AddNodes(UniqueCrossings.Num(), StartIndex);

		UPCGBasePointData* MutablePoints = PointIO->GetOut();
		const int32 NumPoints = Graph->Nodes.Num();
		MutablePoints->SetNumPoints(NumPoints);

		UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;

		TPCGValueRange<int64> MetadataEntries = MutablePoints->GetMetadataEntryValueRange(false);

		TArray<TTuple<int64, int64>> DelayedEntries;
		DelayedEntries.SetNum(NumPoints - StartIndex);

		int32 WriteIndex = 0;
		for (int i = StartIndex; i < NumPoints; i++)
		{
			MetadataEntries[i] = Metadata->AddEntryPlaceholder();
			DelayedEntries[WriteIndex++] = MakeTuple(MetadataEntries[i], PCGInvalidEntryKey);
		}

		Metadata->AddDelayedEntries(DelayedEntries);

		return true;
	}

	void FEdgeEdgeIntersections::InsertEdges()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FEdgeEdgeIntersections::InsertEdges);

		FEdge NewEdge = FEdge{};

		// Find how many new metadata needs to be reserved
		int32 EdgeReserve = 0;
		for (TSharedPtr<FEdgeEdgeProxy>& EdgeProxy : Edges) { EdgeReserve += EdgeProxy->Crossings.Num() + 1; }

		Graph->ReserveForEdges(EdgeReserve, true);

		for (TSharedPtr<FEdgeEdgeProxy>& EdgeProxy : Edges)
		{
			const int32 IOIndex = Graph->Edges[EdgeProxy->Index].IOIndex;
			const int32 EdgeRootIndex = Graph->FindEdgeMetadataRootIndex_Unsafe(EdgeProxy->Index);

			int32 A = EdgeProxy->Start;
			int32 B = -1;

			for (const FEECrossing& Crossing : EdgeProxy->Crossings)
			{
				B = Crossing.Index;

				//BUG: this is the wrong edge IOIndex

				if (Graph->InsertEdge_Unsafe(A, B, NewEdge, IOIndex))
				{
					FGraphEdgeMetadata& NewEdgeMeta = Graph->AddNodeAndEdgeMetadata_Unsafe(B, NewEdge.Index, EdgeRootIndex, EPCGExIntersectionType::EdgeEdge);
					NewEdgeMeta.bIsSubEdge = true;
				}
				else if (FGraphEdgeMetadata* ExistingEdgeMeta = Graph->EdgeMetadata.Find(NewEdge.Index))
				{
					ExistingEdgeMeta->UnionSize++;
					ExistingEdgeMeta->bIsSubEdge = true;
				}

				A = B;
			}

			// Insert last edge
			if (Graph->InsertEdge_Unsafe(A, EdgeProxy->End, NewEdge, IOIndex))
			{
				FGraphEdgeMetadata& NewEdgeMeta = Graph->AddEdgeMetadata_Unsafe(NewEdge.Index, EdgeRootIndex, EPCGExIntersectionType::EdgeEdge);
				NewEdgeMeta.bIsSubEdge = true;
			}
			else if (FGraphEdgeMetadata* ExistingEdgeMeta = Graph->EdgeMetadata.Find(NewEdge.Index))
			{
				ExistingEdgeMeta->UnionSize++;
				ExistingEdgeMeta->bIsSubEdge = true;
			}
		}
	}

	void FEdgeEdgeIntersections::BlendIntersection(
		const int32 Index,
		const TSharedRef<PCGExBlending::FMetadataBlender>& Blender,
		TArray<PCGEx::FOpStats>& Trackers) const
	{
		const FEECrossing& Crossing = UniqueCrossings[Index];

		const int32 Target = Graph->Nodes[Crossing.Index].PointIndex;
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
		const int32 GraphIndex = EdgeProxy->Index;
		const int32 Start = EdgeProxy->Start;
		const int32 End = EdgeProxy->End;

		const TArray<FVector>& Directions = InIntersections->Directions;
		InIntersections->Octree->FindElementsWithBoundsTest(EdgeProxy->Box, [&](const PCGExOctree::FItem& Item)
		{
			const FEdge& OtherEdge = InIntersections->Graph->Edges[Item.Index];
			if (!InIntersections->ValidEdges[Item.Index] || Item.Index == GraphIndex || Start == OtherEdge.Start || Start == OtherEdge.End || End == OtherEdge.End || End == OtherEdge.Start) { return; }

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
		const int32 GraphIndex = EdgeProxy->Index;
		const int32 Start = EdgeProxy->Start;
		const int32 End = EdgeProxy->End;

		const TArray<FVector>& Directions = InIntersections->Directions;

		const int32 RootIndex = InIntersections->Graph->FindEdgeMetadata_Unsafe(GraphIndex)->RootIndex;
		TSharedPtr<PCGExData::FUnionMetadata> EdgesUnion = InIntersections->Graph->EdgesUnion;
		const TSet<int32>& RootIOIndices = EdgesUnion->Entries[RootIndex]->IOSet;

		InIntersections->Octree->FindElementsWithBoundsTest(EdgeProxy->Box, [&](const PCGExOctree::FItem& Item)
		{
			const FEdge& OtherEdge = InIntersections->Graph->Edges[Item.Index];
			if (!InIntersections->ValidEdges[Item.Index] || Item.Index == GraphIndex || Start == OtherEdge.Start || Start == OtherEdge.End || End == OtherEdge.End || End == OtherEdge.Start) { return; }

			if (InIntersections->Details->bUseMinAngle || InIntersections->Details->bUseMaxAngle)
			{
				if (!InIntersections->Details->CheckDot(FMath::Abs(FVector::DotProduct(Directions[GraphIndex], Directions[OtherEdge.Index])))) { return; }
			}

			// Check overlap last as it's the most expensive op
			if (EdgesUnion->IOIndexOverlap(InIntersections->Graph->FindEdgeMetadata_Unsafe(OtherEdge.Index)->RootIndex, RootIOIndices)) { return; }

			EdgeProxy->FindSplit(OtherEdge, InIntersections);
		});
	}

#pragma endregion
}