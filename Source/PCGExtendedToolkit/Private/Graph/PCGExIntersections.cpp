// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExIntersections.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExCluster.h"

namespace PCGExGraph
{
	bool FCompoundNode::Add(FCompoundNode* OtherNode)
	{
		if (OtherNode->Index == Index) { return false; }
		if (Neighbors.Contains(OtherNode->Index)) { return true; }

		Neighbors.Add(OtherNode->Index);
		OtherNode->Add(this);
		return true;
	}

	FVector FCompoundNode::UpdateCenter(const PCGExData::FIdxCompoundList* PointsCompounds, PCGExData::FPointIOCollection* IOGroup)
	{
		Center = FVector::ZeroVector;
		double Divider = 0;

		uint32 IOIndex = 0;
		uint32 PointIndex = 0;

		PCGExData::FIdxCompound* Compound = (*PointsCompounds)[Index];
		for (const uint64 FuseHash : Compound->CompoundedPoints)
		{
			Divider++;
			PCGEx::H64(FuseHash, IOIndex, PointIndex);
			Center += IOGroup->Pairs[IOIndex]->GetInPoint(PointIndex).Transform.GetLocation();
		}

		Center /= Divider;
		return Center;
	}

	FCompoundNode* FCompoundGraph::GetOrCreateNode(const FPCGPoint& Point, const int32 IOIndex, const int32 PointIndex)
	{
		const FVector Origin = Point.Transform.GetLocation();

		if (!bFusePoints)
		{
			FWriteScopeLock WriteLock(OctreeLock);
			FCompoundNode* NewNode = new FCompoundNode(Point, Origin, Nodes.Num());
			Nodes.Add(NewNode);
			PointsCompounds->New()->Add(IOIndex, PointIndex);
			return NewNode;
		}

		int32 Index = -1;
		if (FuseSettings.bComponentWiseTolerance)
		{
			FReadScopeLock ReadLock(OctreeLock);
			const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Origin, FuseSettings.Tolerances);
			Octree.FindFirstElementWithBoundsTest(
				Box, [&](const FCompoundNode* Node)
				{
					if (FuseSettings.IsWithinToleranceComponentWise(Point, Node->Point))
					{
						Index = Node->Index;
						return false;
					}
					return true;
				});

			if (Index != -1)
			{
				PointsCompounds->Add(Index, IOIndex, PointIndex);
				return Nodes[Index];
			}
		}
		else
		{
			FReadScopeLock ReadLock(OctreeLock);
			const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Origin, FVector(FuseSettings.Tolerance));
			Octree.FindFirstElementWithBoundsTest(
				Box, [&](const FCompoundNode* Node)
				{
					if (FuseSettings.IsWithinToleranceComponentWise(Point, Node->Point))
					{
						Index = Node->Index;
						return false;
					}
					return true;
				});

			if (Index != -1)
			{
				PointsCompounds->Add(Index, IOIndex, PointIndex);
				return Nodes[Index];
			}
		}

		FCompoundNode* NewNode;

		{
			FWriteScopeLock WriteLock(OctreeLock);
			NewNode = new FCompoundNode(Point, Origin, Nodes.Num());
			Nodes.Add(NewNode);
			Octree.AddElement(NewNode);
			PointsCompounds->New()->Add(IOIndex, PointIndex);
		}

		return NewNode;
	}

	FCompoundNode* FCompoundGraph::GetOrCreateNodeUnsafe(const FPCGPoint& Point, const int32 IOIndex, const int32 PointIndex)
	{
		const FVector Origin = Point.Transform.GetLocation();

		if (!bFusePoints)
		{
			FCompoundNode* NewNode = new FCompoundNode(Point, Origin, Nodes.Num());
			Nodes.Add(NewNode);
			PointsCompounds->New()->Add(IOIndex, PointIndex);
			return NewNode;
		}

		int32 Index = -1;
		if (FuseSettings.bComponentWiseTolerance)
		{
			const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Origin, FuseSettings.Tolerances);
			Octree.FindFirstElementWithBoundsTest(
				Box, [&](const FCompoundNode* Node)
				{
					if (FuseSettings.IsWithinToleranceComponentWise(Point, Node->Point))
					{
						Index = Node->Index;
						return false;
					}
					return true;
				});

			if (Index != -1)
			{
				PointsCompounds->Add(Index, IOIndex, PointIndex);
				return Nodes[Index];
			}
		}
		else
		{
			const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Origin, FVector(FuseSettings.Tolerance));
			Octree.FindFirstElementWithBoundsTest(
				Box, [&](const FCompoundNode* Node)
				{
					if (FuseSettings.IsWithinToleranceComponentWise(Point, Node->Point))
					{
						Index = Node->Index;
						return false;
					}
					return true;
				});

			if (Index != -1)
			{
				PointsCompounds->Add(Index, IOIndex, PointIndex);
				return Nodes[Index];
			}
		}

		FCompoundNode* NewNode = new FCompoundNode(Point, Origin, Nodes.Num());
		Nodes.Add(NewNode);
		Octree.AddElement(NewNode);
		PointsCompounds->New()->Add(IOIndex, PointIndex);

		return NewNode;
	}

	PCGExData::FIdxCompound* FCompoundGraph::CreateBridge(const FPCGPoint& From, const int32 FromIOIndex, const int32 FromPointIndex, const FPCGPoint& To, const int32 ToIOIndex, const int32 ToPointIndex, const int32 EdgeIOIndex, const int32 EdgePointIndex)
	{
		FCompoundNode* StartVtx = GetOrCreateNode(From, FromIOIndex, FromPointIndex);
		FCompoundNode* EndVtx = GetOrCreateNode(To, ToIOIndex, ToPointIndex);
		PCGExData::FIdxCompound* EdgeIdx = nullptr;


		StartVtx->Add(EndVtx);
		EndVtx->Add(StartVtx);

		if (EdgeIOIndex == -1) { return EdgeIdx; } // Skip edge management

		const uint64 H = PCGEx::H64U(StartVtx->Index, EndVtx->Index);
		if (const FIndexedEdge* Edge = Edges.Find(H))
		{
			EdgeIdx = EdgesCompounds->Compounds[Edge->EdgeIndex];
			EdgeIdx->Add(EdgeIOIndex, EdgePointIndex);
		}
		else
		{
			EdgeIdx = EdgesCompounds->New();
			Edges.Add(H, FIndexedEdge(Edges.Num(), StartVtx->Index, EndVtx->Index));
			EdgeIdx->Add(EdgeIOIndex, EdgePointIndex);
		}

		return EdgeIdx;
	}

	PCGExData::FIdxCompound* FCompoundGraph::CreateBridgeUnsafe(const FPCGPoint& From, const int32 FromIOIndex, const int32 FromPointIndex, const FPCGPoint& To, const int32 ToIOIndex, const int32 ToPointIndex, const int32 EdgeIOIndex, const int32 EdgePointIndex)
	{
		FCompoundNode* StartVtx = GetOrCreateNodeUnsafe(From, FromIOIndex, FromPointIndex);
		FCompoundNode* EndVtx = GetOrCreateNodeUnsafe(To, ToIOIndex, ToPointIndex);
		PCGExData::FIdxCompound* EdgeIdx = nullptr;

		StartVtx->Add(EndVtx);
		EndVtx->Add(StartVtx);

		if (EdgeIOIndex == -1) { return EdgeIdx; } // Skip edge management

		const uint64 H = PCGEx::H64U(StartVtx->Index, EndVtx->Index);
		if (const FIndexedEdge* Edge = Edges.Find(H))
		{
			EdgeIdx = EdgesCompounds->Compounds[Edge->EdgeIndex];
			EdgeIdx->Add(EdgeIOIndex, EdgePointIndex);
		}
		else
		{
			EdgeIdx = EdgesCompounds->New();
			Edges.Add(H, FIndexedEdge(Edges.Num(), StartVtx->Index, EndVtx->Index));
			EdgeIdx->Add(EdgeIOIndex, EdgePointIndex);
		}

		return EdgeIdx;
	}

	void FCompoundGraph::GetUniqueEdges(TArray<FUnsignedEdge>& OutEdges)
	{
		OutEdges.Empty(Nodes.Num() * 4);
		TSet<uint64> UniqueEdges;
		for (const FCompoundNode* Node : Nodes)
		{
			for (const int32 OtherNodeIndex : Node->Neighbors)
			{
				const uint64 Hash = PCGEx::H64U(Node->Index, OtherNodeIndex);
				if (UniqueEdges.Contains(Hash)) { continue; }
				UniqueEdges.Add(Hash);
				OutEdges.Emplace(Node->Index, OtherNodeIndex);
			}
		}
		UniqueEdges.Empty();
	}

	void FCompoundGraph::WriteMetadata(TMap<int32, FGraphNodeMetadata*>& OutMetadata)
	{
		for (const FCompoundNode* Node : Nodes)
		{
			FGraphNodeMetadata* NodeMeta = FGraphNodeMetadata::GetOrCreate(Node->Index, OutMetadata);
			NodeMeta->CompoundSize = Node->Neighbors.Num();
			NodeMeta->bCompounded = NodeMeta->CompoundSize > 1;
		}
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
		FGraph* InGraph,
		FCompoundGraph* InCompoundGraph,
		PCGExData::FPointIO* InPointIO,
		const FPCGExPointEdgeIntersectionSettings& InSettings)
		: PointIO(InPointIO), Graph(InGraph), CompoundGraph(InCompoundGraph), Settings(InSettings)
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
				Settings.FuseSettings.Tolerance);
		}
	}

	void FPointEdgeIntersections::FindIntersections(FPCGExPointsProcessorContext* InContext)
	{
		for (const FIndexedEdge& Edge : Graph->Edges)
		{
			if (!Edge.bValid) { continue; }
			InContext->GetAsyncManager()->Start<PCGExGraphTask::FFindPointEdgeIntersections>(Edge.EdgeIndex, PointIO, this);
		}
	}

	void FPointEdgeIntersections::Add(const int32 EdgeIndex, const FPESplit& Split)
	{
		FWriteScopeLock WriteLock(InsertionLock);
		Edges[EdgeIndex].CollinearPoints.AddUnique(Split);
	}

	void FPointEdgeIntersections::Insert()
	{
		FIndexedEdge NewEdge = FIndexedEdge{};

		for (FPointEdgeProxy& PointEdgeProxy : Edges)
		{
			if (PointEdgeProxy.CollinearPoints.IsEmpty()) { continue; }

			FIndexedEdge& SplitEdge = Graph->Edges[PointEdgeProxy.EdgeIndex];
			SplitEdge.bValid = false; // Invalidate existing edge
			PointEdgeProxy.CollinearPoints.Sort([](const FPESplit& A, const FPESplit& B) { return A.Time < B.Time; });

			const int32 FirstIndex = SplitEdge.Start;
			const int32 LastIndex = SplitEdge.End;

			int32 NodeIndex = -1;

			int32 PrevIndex = FirstIndex;
			for (const FPESplit Split : PointEdgeProxy.CollinearPoints)
			{
				NodeIndex = Split.NodeIndex;

				Graph->InsertEdge(PrevIndex, NodeIndex, NewEdge, SplitEdge.IOIndex); //TODO: IOIndex required
				PrevIndex = NodeIndex;

				FGraphNodeMetadata* NodeMetadata = FGraphNodeMetadata::GetOrCreate(NodeIndex, Graph->NodeMetadata);
				NodeMetadata->Type = EPCGExIntersectionType::PointEdge;

				FGraphEdgeMetadata* EdgeMetadata = FGraphEdgeMetadata::GetOrCreate(NewEdge.EdgeIndex, SplitEdge.EdgeIndex, Graph->EdgeMetadata);
				EdgeMetadata->Type = EPCGExIntersectionType::PointEdge;

				if (Settings.bSnapOnEdge)
				{
					PointIO->GetMutablePoint(Graph->Nodes[Split.NodeIndex].PointIndex).Transform.SetLocation(Split.ClosestPoint);
				}
			}

			Graph->InsertEdge(NodeIndex, LastIndex, NewEdge, SplitEdge.IOIndex); // Insert last edge
		}
	}

	void FPointEdgeIntersections::BlendIntersection(const int32 Index, PCGExDataBlending::FMetadataBlender* Blender) const
	{
		const FPointEdgeProxy& PointEdgeProxy = Edges[Index];

		if (PointEdgeProxy.CollinearPoints.IsEmpty()) { return; }

		const FIndexedEdge& SplitEdge = Graph->Edges[PointEdgeProxy.EdgeIndex];

		const PCGEx::FPointRef A = PointIO->GetOutPointRef(SplitEdge.Start);
		const PCGEx::FPointRef B = PointIO->GetOutPointRef(SplitEdge.End);

		for (const FPESplit Split : PointEdgeProxy.CollinearPoints)
		{
			const PCGEx::FPointRef Target = PointIO->GetOutPointRef(Graph->Nodes[Split.NodeIndex].PointIndex);
			FPCGPoint& Pt = PointIO->GetMutablePoint(Target.Index);

			FVector PreBlendLocation = Pt.Transform.GetLocation();

			Blender->PrepareForBlending(Target);
			Blender->Blend(A, B, Target, 0.5);
			Blender->CompleteBlending(Target, 2, 1);

			Pt.Transform.SetLocation(PreBlendLocation);
		}
	}

	bool FEdgeEdgeProxy::FindSplit(const FEdgeEdgeProxy& OtherEdge, FEESplit& OutSplit) const
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

		OutSplit.Center = FMath::Lerp(A, B, 0.5);
		OutSplit.TimeA = FVector::DistSquared(Start, A) / LengthSquared;
		OutSplit.TimeB = FVector::DistSquared(OtherEdge.Start, B) / OtherEdge.LengthSquared;

		return true;
	}

	FEdgeEdgeIntersections::FEdgeEdgeIntersections(
		FGraph* InGraph,
		FCompoundGraph* InCompoundGraph,
		PCGExData::FPointIO* InPointIO,
		const FPCGExEdgeEdgeIntersectionSettings& InSettings)
		: PointIO(InPointIO), Graph(InGraph), CompoundGraph(InCompoundGraph), Settings(InSettings)
	{
		const TArray<FPCGPoint>& Points = InPointIO->GetOutIn()->GetPoints();

		const int32 NumEdges = InGraph->Edges.Num();
		Edges.SetNum(NumEdges);

		Octree = TEdgeOctree(InCompoundGraph->Bounds.GetCenter(), InCompoundGraph->Bounds.GetExtent().Length() + (Settings.Tolerance * 2));

		for (const FIndexedEdge& Edge : InGraph->Edges)
		{
			if (!Edge.bValid) { continue; }
			Edges[Edge.EdgeIndex].Init(
				Edge.EdgeIndex,
				Points[Edge.Start].Transform.GetLocation(),
				Points[Edge.End].Transform.GetLocation(),
				Settings.Tolerance);

			Octree.AddElement(&Edges[Edge.EdgeIndex]);
		}
	}

	void FEdgeEdgeIntersections::FindIntersections(FPCGExPointsProcessorContext* InContext)
	{
		for (const FIndexedEdge& Edge : Graph->Edges)
		{
			if (!Edge.bValid) { continue; }
			InContext->GetAsyncManager()->Start<PCGExGraphTask::FFindEdgeEdgeIntersections>(Edge.EdgeIndex, PointIO, this);
		}
	}

	void FEdgeEdgeIntersections::Add(const int32 EdgeIndex, const int32 OtherEdgeIndex, const FEESplit& Split)
	{
		FWriteScopeLock WriteLock(InsertionLock);

		CheckedPairs.Add(PCGEx::H64U(EdgeIndex, OtherEdgeIndex));

		FEECrossing* OutSplit = new FEECrossing(Split);

		OutSplit->NodeIndex = Crossings.Add(OutSplit) + Graph->Nodes.Num();
		OutSplit->EdgeA = FMath::Min(EdgeIndex, OtherEdgeIndex);
		OutSplit->EdgeB = FMath::Max(EdgeIndex, OtherEdgeIndex);

		Edges[EdgeIndex].Intersections.AddUnique(OutSplit);
		Edges[OtherEdgeIndex].Intersections.AddUnique(OutSplit);
	}

	void FEdgeEdgeIntersections::Insert()
	{
		FIndexedEdge NewEdge = FIndexedEdge{};

		// Insert new nodes
		const TArrayView<FNode> NewNodes = Graph->AddNodes(Crossings.Num());

		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
		MutablePoints.SetNum(Graph->Nodes.Num());

		// Now handled by intersection blending
		//for (int i = 0; i < NewNodes.Num(); i++) { MutablePoints[NewNodes[i].NodeIndex].Transform.SetLocation(Crossings[i]->Split.Center); }

		for (FEdgeEdgeProxy& EdgeProxy : Edges)
		{
			if (EdgeProxy.Intersections.IsEmpty()) { continue; }

			FIndexedEdge& SplitEdge = Graph->Edges[EdgeProxy.EdgeIndex];
			SplitEdge.bValid = false; // Invalidate existing edge

			EdgeProxy.Intersections.Sort(
				[&](const FEECrossing& A, const FEECrossing& B)
				{
					return A.GetTime(EdgeProxy.EdgeIndex) > B.GetTime(EdgeProxy.EdgeIndex);
				});

			const int32 FirstIndex = SplitEdge.Start;
			const int32 LastIndex = SplitEdge.End;

			int32 NodeIndex = -1;

			int32 PrevIndex = FirstIndex;
			for (const FEECrossing* Crossing : EdgeProxy.Intersections)
			{
				NodeIndex = Crossing->NodeIndex;
				Graph->InsertEdge(PrevIndex, NodeIndex, NewEdge, SplitEdge.IOIndex); //TODO: this is the wrong edge IOIndex
				PrevIndex = NodeIndex;

				FGraphNodeMetadata* NodeMetadata = FGraphNodeMetadata::GetOrCreate(NodeIndex, Graph->NodeMetadata);
				NodeMetadata->Type = EPCGExIntersectionType::EdgeEdge;

				FGraphEdgeMetadata* EdgeMetadata = FGraphEdgeMetadata::GetOrCreate(NewEdge.EdgeIndex, EdgeProxy.EdgeIndex, Graph->EdgeMetadata);
				EdgeMetadata->Type = EPCGExIntersectionType::EdgeEdge;
			}

			Graph->InsertEdge(NodeIndex, LastIndex, NewEdge, SplitEdge.IOIndex); // Insert last edge
		}
	}

	void FEdgeEdgeIntersections::BlendIntersection(const int32 Index, PCGExDataBlending::FMetadataBlender* Blender) const
	{
		const FEECrossing* Crossing = Crossings[Index];

		const PCGEx::FPointRef Target = PointIO->GetOutPointRef(Graph->Nodes[Crossing->NodeIndex].PointIndex);
		Blender->PrepareForBlending(Target);

		const PCGEx::FPointRef A1 = PointIO->GetOutPointRef(Graph->Nodes[Graph->Edges[Crossing->EdgeA].Start].PointIndex);
		const PCGEx::FPointRef A2 = PointIO->GetOutPointRef(Graph->Nodes[Graph->Edges[Crossing->EdgeA].End].PointIndex);
		const PCGEx::FPointRef B1 = PointIO->GetOutPointRef(Graph->Nodes[Graph->Edges[Crossing->EdgeB].Start].PointIndex);
		const PCGEx::FPointRef B2 = PointIO->GetOutPointRef(Graph->Nodes[Graph->Edges[Crossing->EdgeB].End].PointIndex);

		Blender->Blend(Target, A1, Target, Crossing->Split.TimeA);
		Blender->Blend(Target, A2, Target, 1 - Crossing->Split.TimeA);
		Blender->Blend(Target, B1, Target, Crossing->Split.TimeB);
		Blender->Blend(Target, B2, Target, 1 - Crossing->Split.TimeB);

		Blender->CompleteBlending(Target, 4, 2);

		PointIO->GetMutablePoint(Target.Index).Transform.SetLocation(Crossing->Split.Center);
	}
}

namespace PCGExGraphTask
{
	bool FFindPointEdgeIntersections::ExecuteTask()
	{
		FindCollinearNodes(IntersectionList, TaskIndex, PointIO->GetOutIn());
		return true;
	}

	bool FInsertPointEdgeIntersections::ExecuteTask()
	{
		IntersectionList->Insert();
		return true;
	}

	bool FFindEdgeEdgeIntersections::ExecuteTask()
	{
		FindOverlappingEdges(IntersectionList, TaskIndex);
		return true;
	}

	bool FInsertEdgeEdgeIntersections::ExecuteTask()
	{
		IntersectionList->Insert();
		return true;
	}

	bool FCompoundGraphInsertPoints::ExecuteTask()
	{
		PointIO->CreateInKeys();

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();

		TArray<int32> InSorted;
		InSorted.SetNum(InPoints.Num());
		for (int i = 0; i < InSorted.Num(); i++) { InSorted[i] = i; }

		/*
		InSorted.Sort(
			[&](const int32 A, const int32 B)
			{
				const FVector V = InPoints[A].Transform.GetLocation() - InPoints[B].Transform.GetLocation();
				return FMath::IsNearlyZero(V.X) ? FMath::IsNearlyZero(V.Y) ? V.Z > 0 : V.Y > 0 : V.X > 0;
			});
		*/

		for (const int32 PointIndex : InSorted) { Graph->GetOrCreateNode(InPoints[PointIndex], PointIO->IOIndex, PointIndex); }

		/*
		if (Context->bPreserveOrder)
		{
			FusedPoints.Sort([&](const PCGExFuse::FFusedPoint& A, const PCGExFuse::FFusedPoint& B) { return A.Index > B.Index; });
		}
		*/
		return true;
	}

	bool FCompoundGraphInsertEdges::ExecuteTask()
	{
		TArray<PCGExGraph::FIndexedEdge> IndexedEdges;
		if (!BuildIndexedEdges(*EdgeIO, *NodeIndicesMap, IndexedEdges, true) ||
			IndexedEdges.IsEmpty()) { return false; }

		IndexedEdges.Sort(
			[&](const PCGExGraph::FIndexedEdge& A, const PCGExGraph::FIndexedEdge& B)
			{
				const FVector AStart = PointIO->GetInPoint(A.Start).Transform.GetLocation();
				const FVector BStart = PointIO->GetInPoint(B.Start).Transform.GetLocation();

				int32 Result = AStart.X == BStart.X ? 0 : AStart.X < BStart.X ? 1 : -1;
				if (Result != 0) { return Result == 1; }

				Result = AStart.Y == BStart.Y ? 0 : AStart.Y < BStart.Y ? 1 : -1;
				if (Result != 0) { return Result == 1; }

				Result = AStart.Z == BStart.Z ? 0 : AStart.Z < BStart.Z ? 1 : -1;
				if (Result != 0) { return Result == 1; }

				const FVector AEnd = PointIO->GetInPoint(A.End).Transform.GetLocation();
				const FVector BEnd = PointIO->GetInPoint(B.End).Transform.GetLocation();

				Result = AEnd.X == BEnd.X ? 0 : AEnd.X < BEnd.X ? 1 : -1;
				if (Result != 0) { return Result == 1; }

				Result = AEnd.Y == BEnd.Y ? 0 : AEnd.Y < BEnd.Y ? 1 : -1;
				if (Result != 0) { return Result == 1; }

				Result = AEnd.Z == BEnd.Z ? 0 : AEnd.Z < BEnd.Z ? 1 : -1;
				return Result == 1;
			});

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		for (const PCGExGraph::FIndexedEdge& Edge : IndexedEdges)
		{
			Graph->CreateBridgeUnsafe(
				InPoints[Edge.Start], TaskIndex, Edge.Start,
				InPoints[Edge.End], TaskIndex, Edge.End, EdgeIO->IOIndex, Edge.PointIndex);
		}

		return true;
	}
}
