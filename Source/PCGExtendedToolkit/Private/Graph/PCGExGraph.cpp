// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExGraph.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExCluster.h"

namespace PCGExGraph
{
	void FNode::Add(const int32 EdgeIndex) { Edges.AddUnique(EdgeIndex); }

	void FSubGraph::Add(const FIndexedEdge& Edge, FGraph* InGraph)
	{
		Nodes.Add(Edge.Start);
		Nodes.Add(Edge.End);

		Edges.Add(Edge.EdgeIndex);
		if (Edge.IOIndex >= 0) { EdgesInIOIndices.Add(Edge.IOIndex); }
	}

	void FSubGraph::Invalidate(FGraph* InGraph)
	{
		for (const int32 EdgeIndex : Edges) { InGraph->Edges[EdgeIndex].bValid = false; }
		for (const int32 NodeIndex : Nodes) { InGraph->Nodes[NodeIndex].bValid = false; }
	}

	int32 FSubGraph::GetFirstInIOIndex()
	{
		for (const int32 InIOIndex : EdgesInIOIndices) { return InIOIndex; }
		return -1;
	}

	bool FGraph::InsertEdge(const int32 A, const int32 B, FIndexedEdge& OutEdge)
	{
		const uint64 Hash = PCGEx::H64U(A, B);

		{
			FReadScopeLock ReadLock(GraphLock);
			if (UniqueEdges.Contains(Hash)) { return false; }
		}

		FWriteScopeLock WriteLock(GraphLock);

		UniqueEdges.Add(Hash);

		OutEdge = Edges.Emplace_GetRef(Edges.Num(), A, B);

		Nodes[A].Add(OutEdge.EdgeIndex);
		Nodes[B].Add(OutEdge.EdgeIndex);

		return true;
	}

	bool FGraph::InsertEdge(const FIndexedEdge& Edge)
	{
		const uint64 Hash = Edge.H64U();

		{
			FReadScopeLock ReadLock(GraphLock);
			if (UniqueEdges.Contains(Hash)) { return false; }
		}

		FWriteScopeLock WriteLock(GraphLock);

		UniqueEdges.Add(Hash);

		FIndexedEdge& NewEdge = Edges.Emplace_GetRef(Edge);
		NewEdge.EdgeIndex = Edges.Num() - 1;

		Nodes[Edge.Start].Add(NewEdge.EdgeIndex);
		Nodes[Edge.End].Add(NewEdge.EdgeIndex);

		return true;
	}

	void FGraph::InsertEdges(const TArray<uint64>& InEdges, const int32 InIOIndex)
	{
		FWriteScopeLock WriteLock(GraphLock);
		uint32 A;
		uint32 B;
		for (const uint64& E : InEdges)
		{
			if (UniqueEdges.Contains(E)) { continue; }

			UniqueEdges.Add(E);
			PCGEx::H64(E, A, B);
			const int32 EdgeIndex = Edges.Emplace(Edges.Num(), A, B);
			Nodes[A].Add(EdgeIndex);
			Nodes[B].Add(EdgeIndex);
			Edges[EdgeIndex].IOIndex = InIOIndex;
		}
	}

	void FGraph::InsertEdges(const TSet<uint64>& InEdges, const int32 InIOIndex)
	{
		FWriteScopeLock WriteLock(GraphLock);
		uint32 A;
		uint32 B;
		for (const uint64& E : InEdges)
		{
			if (UniqueEdges.Contains(E)) { continue; }

			UniqueEdges.Add(E);
			PCGEx::H64(E, A, B);
			const int32 EdgeIndex = Edges.Emplace(Edges.Num(), A, B);
			Nodes[A].Add(EdgeIndex);
			Nodes[B].Add(EdgeIndex);
			Edges[EdgeIndex].IOIndex = InIOIndex;
		}
	}

#define PCGEX_EDGE_INSERT\
	if (!E.bValid || !Nodes.IsValidIndex(E.Start) || !Nodes.IsValidIndex(E.End)) { continue; }\
	const uint64 Hash = E.H64U(); if (UniqueEdges.Contains(Hash)) { continue; }\
	UniqueEdges.Add(Hash); const FIndexedEdge& Edge = Edges.Emplace_GetRef(Edges.Num(), E.Start, E.End);\
	Nodes[E.Start].Add(Edge.EdgeIndex);	Nodes[E.End].Add(Edge.EdgeIndex);

	void FGraph::InsertEdges(const TArray<FUnsignedEdge>& InEdges, const int32 InIOIndex)
	{
		FWriteScopeLock WriteLock(GraphLock);
		for (const FUnsignedEdge& E : InEdges)
		{
			PCGEX_EDGE_INSERT
			Edges[Edge.EdgeIndex].IOIndex = InIOIndex;
		}
	}

	void FGraph::InsertEdges(const TArray<FIndexedEdge>& InEdges)
	{
		FWriteScopeLock WriteLock(GraphLock);
		for (const FIndexedEdge& E : InEdges)
		{
			PCGEX_EDGE_INSERT
			Edges[Edge.EdgeIndex].IOIndex = E.IOIndex;
			Edges[Edge.EdgeIndex].PointIndex = E.PointIndex;
		}
	}

#undef PCGEX_EDGE_INSERT

	TArrayView<FNode> FGraph::AddNodes(const int32 NumNewNodes)
	{
		const int32 StartIndex = Nodes.Num();
		Nodes.SetNum(StartIndex + NumNewNodes);
		for (int i = 0; i < NumNewNodes; i++)
		{
			FNode& Node = Nodes[StartIndex + i];
			Node.NodeIndex = Node.PointIndex = StartIndex + i;
			Node.Edges.Reserve(NumEdgesReserve);
		}

		return MakeArrayView(Nodes.GetData() + StartIndex, NumNewNodes);
	}


	void FGraph::BuildSubGraphs(const int32 Min, const int32 Max)
	{
		TSet<int32> VisitedNodes;
		VisitedNodes.Reserve(Nodes.Num());

		for (int i = 0; i < Nodes.Num(); i++)
		{
			if (VisitedNodes.Contains(i)) { continue; }

			const FNode& CurrentNode = Nodes[i];
			if (!CurrentNode.bValid || // Points are valid by default, but may be invalidated prior to building the subgraph
				CurrentNode.Edges.IsEmpty())
			{
				VisitedNodes.Add(i);
				continue;
			}

			FSubGraph* SubGraph = new FSubGraph();

			TQueue<int32> Queue;
			Queue.Enqueue(i);

			int32 NextIndex = -1;
			while (Queue.Dequeue(NextIndex))
			{
				if (VisitedNodes.Contains(NextIndex)) { continue; }
				VisitedNodes.Add(NextIndex);

				FNode& Node = Nodes[NextIndex];
				Node.NumExportedEdges = 0;

				for (const int32 E : Node.Edges)
				{
					const FIndexedEdge& Edge = Edges[E];
					if (!Edge.bValid) { continue; }

					int32 OtherIndex = Edge.Other(NextIndex);
					if (!Nodes[OtherIndex].bValid) { continue; }

					Node.NumExportedEdges++;
					SubGraph->Add(Edge, this);
					if (!VisitedNodes.Contains(OtherIndex)) { Queue.Enqueue(OtherIndex); }
				}
			}

			if (!FMath::IsWithin(SubGraph->Edges.Num(), FMath::Max(Min, 1), FMath::Max(Max, 1)))
			{
				SubGraph->Invalidate(this); // Will invalidate isolated points
				delete SubGraph;
			}
			else
			{
				SubGraphs.Add(SubGraph);
			}
		}
	}

	void FGraph::GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth) const
	{
		const int32 NextDepth = SearchDepth - 1;
		const FNode& RootNode = Nodes[FromIndex];

		for (const int32 EdgeIndex : RootNode.Edges)
		{
			const FIndexedEdge& Edge = Edges[EdgeIndex];
			if (!Edge.bValid) { continue; }

			int32 OtherIndex = Edge.Other(FromIndex);
			if (OutIndices.Contains(OtherIndex)) { continue; }

			OutIndices.Add(OtherIndex);
			if (NextDepth > 0) { GetConnectedNodes(OtherIndex, OutIndices, NextDepth); }
		}
	}

	void FGraphBuilder::Compile(FPCGExPointsProcessorContext* InContext,
	                            FGraphMetadataSettings* MetadataSettings) const
	{
		InContext->GetAsyncManager()->Start<PCGExGraphTask::FCompileGraph>(
			-1, PointIO, const_cast<FGraphBuilder*>(this),
			OutputSettings->GetMinClusterSize(), OutputSettings->GetMaxClusterSize(), MetadataSettings);
	}

	void FGraphBuilder::Write(FPCGExPointsProcessorContext* InContext) const
	{
		EdgesIO->OutputTo(InContext);
	}


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

				Graph->InsertEdge(PrevIndex, NodeIndex, NewEdge);
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

			Graph->InsertEdge(NodeIndex, LastIndex, NewEdge); // Insert last edge
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
		for (int i = 0; i < NewNodes.Num(); i++)
		{
			MutablePoints[NewNodes[i].NodeIndex].Transform.SetLocation(Crossings[i]->Split.Center);
		}

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
				Graph->InsertEdge(PrevIndex, NodeIndex, NewEdge);
				PrevIndex = NodeIndex;

				FGraphNodeMetadata* NodeMetadata = FGraphNodeMetadata::GetOrCreate(NodeIndex, Graph->NodeMetadata);
				NodeMetadata->Type = EPCGExIntersectionType::EdgeEdge;

				FGraphEdgeMetadata* EdgeMetadata = FGraphEdgeMetadata::GetOrCreate(NewEdge.EdgeIndex, EdgeProxy.EdgeIndex, Graph->EdgeMetadata);
				EdgeMetadata->Type = EPCGExIntersectionType::EdgeEdge;
			}

			Graph->InsertEdge(NodeIndex, LastIndex, NewEdge); // Insert last edge
		}
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

	bool FWriteSubGraphEdges::ExecuteTask()
	{
		WriteSubGraphEdges(PointIO->GetOut()->GetPoints(), Graph, SubGraph, MetadataSettings);
		return true;
	}

	bool FCompileGraph::ExecuteTask()
	{
		Builder->Graph->BuildSubGraphs(Min, Max);

		if (Builder->Graph->SubGraphs.IsEmpty())
		{
			Builder->bCompiledSuccessfully = false;
			return false;
		}

		PointIO->Cleanup(); //Ensure fresh keys later on

		TArray<PCGExGraph::FNode>& Nodes = Builder->Graph->Nodes;
		TArray<int32> ValidNodes;
		ValidNodes.Reserve(Builder->Graph->Nodes.Num());

		if (Builder->bPrunePoints)
		{
			// Rebuild point list with only the one used
			// to know which are used, we need to prune subgraphs first
			TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();

			if (!MutablePoints.IsEmpty())
			{
				//Assume points were filled before, and remove them from the current array
				TArray<FPCGPoint> PrunedPoints;
				PrunedPoints.Reserve(MutablePoints.Num());

				for (PCGExGraph::FNode& Node : Nodes)
				{
					if (!Node.bValid || Node.Edges.IsEmpty()) { continue; }
					Node.PointIndex = PrunedPoints.Add(MutablePoints[Node.PointIndex]);
					ValidNodes.Add(Node.NodeIndex);
				}

				PointIO->GetOut()->SetPoints(PrunedPoints);
			}
			else
			{
				const int32 NumMaxNodes = Nodes.Num();
				MutablePoints.Reserve(NumMaxNodes);

				for (PCGExGraph::FNode& Node : Nodes)
				{
					if (!Node.bValid || Node.Edges.IsEmpty()) { continue; }
					Node.PointIndex = MutablePoints.Add(PointIO->GetInPoint(Node.PointIndex));
					ValidNodes.Add(Node.NodeIndex);
				}
			}
		}
		else
		{
			for (const PCGExGraph::FNode& Node : Nodes) { if (Node.bValid) { ValidNodes.Add(Node.NodeIndex); } }
		}

		PointIO->SetNumInitialized(PointIO->GetOutNum(), true);

		PCGEx::TFAttributeWriter<int64>* IndexWriter = new PCGEx::TFAttributeWriter<int64>(PCGExGraph::Tag_EdgeIndex, -1, false);
		PCGEx::TFAttributeWriter<int32>* NumEdgesWriter = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::Tag_EdgesNum, 0, false);

		IndexWriter->BindAndGet(*PointIO);
		NumEdgesWriter->BindAndGet(*PointIO);

		for (int i = 0; i < IndexWriter->Values.Num(); i++) { IndexWriter->Values[i] = PointIO->GetOutPoint(i).MetadataEntry; }
		for (const int32 NodeIndex : ValidNodes)
		{
			const PCGExGraph::FNode& Node = Nodes[NodeIndex];
			NumEdgesWriter->Values[Node.PointIndex] = Node.NumExportedEdges;
		}

		IndexWriter->Write();
		NumEdgesWriter->Write();

		PCGEX_DELETE(IndexWriter)
		PCGEX_DELETE(NumEdgesWriter)

		if (MetadataSettings && !Builder->Graph->NodeMetadata.IsEmpty())
		{
#define PCGEX_METADATA(_NAME, _TYPE, _DEFAULT, _ACCESSOR)\
{if(MetadataSettings->bWrite##_NAME){\
PCGEx::TFAttributeWriter<_TYPE>* Writer = MetadataSettings->bWrite##_NAME ? new PCGEx::TFAttributeWriter<_TYPE>(MetadataSettings->_NAME##AttributeName, _DEFAULT, false) : nullptr;\
Writer->BindAndGet(*PointIO);\
		for(const int32 NodeIndex : ValidNodes){\
		PCGExGraph::FGraphNodeMetadata** NodeMeta = Builder->Graph->NodeMetadata.Find(NodeIndex);\
		if(NodeMeta){Writer->Values[Nodes[NodeIndex].PointIndex] = (*NodeMeta)->_ACCESSOR; }}\
		Writer->Write(); delete Writer; }}

			PCGEX_METADATA(Compounded, bool, false, bCompounded)
			PCGEX_METADATA(CompoundSize, int32, 0, CompoundSize)
			PCGEX_METADATA(Intersector, bool, false, IsIntersector())
			PCGEX_METADATA(Crossing, bool, false, IsCrossing())

#undef PCGEX_METADATA
		}

		Builder->bCompiledSuccessfully = true;

		PCGEx::TFAttributeWriter<int64>* NumClusterIdWriter = new PCGEx::TFAttributeWriter<int64>(PCGExGraph::Tag_ClusterId, -1, false);
		NumClusterIdWriter->BindAndGet(*PointIO);

		int32 SubGraphIndex = 0;
		for (PCGExGraph::FSubGraph* SubGraph : Builder->Graph->SubGraphs)
		{
			PCGExData::FPointIO* EdgeIO;

			if (const int32 IOIndex = SubGraph->GetFirstInIOIndex();
				Builder->SourceEdgesIO && Builder->SourceEdgesIO->Pairs.IsValidIndex(IOIndex))
			{
				EdgeIO = &Builder->EdgesIO->Emplace_GetRef(*Builder->SourceEdgesIO->Pairs[IOIndex], PCGExData::EInit::NewOutput);
			}
			else
			{
				EdgeIO = &Builder->EdgesIO->Emplace_GetRef(PCGExData::EInit::NewOutput);
			}

			const int64 ClusterId = EdgeIO->GetOut()->UID;
			SubGraph->PointIO = EdgeIO;

			EdgeIO->Tags->Set(PCGExGraph::TagStr_ClusterPair, Builder->PairIdStr);
			PCGExData::WriteMark(EdgeIO->GetOut()->Metadata, PCGExGraph::Tag_ClusterId, ClusterId);

			for (const int32 EdgeIndex : SubGraph->Edges)
			{
				const PCGExGraph::FIndexedEdge& Edge = Builder->Graph->Edges[EdgeIndex];
				NumClusterIdWriter->Values[Builder->Graph->Nodes[Edge.Start].PointIndex] = ClusterId;
				NumClusterIdWriter->Values[Builder->Graph->Nodes[Edge.End].PointIndex] = ClusterId;
			}

			//Manager->Start<FWriteSubGraphEdges>(SubGraphIndex++, PointIO, Builder->Graph, SubGraph, MetadataSettings);
			WriteSubGraphEdges(PointIO->GetOut()->GetPoints(), Builder->Graph, SubGraph, MetadataSettings);
		}

		NumClusterIdWriter->Write();

		PointIO->Tags->Set(PCGExGraph::TagStr_ClusterPair, Builder->PairIdStr);
		PCGEX_DELETE(NumClusterIdWriter)

		return true;
	}

	bool FCompoundGraphInsertPoints::ExecuteTask()
	{
		PointIO->CreateInKeys();

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();

		TArray<int32> InSorted;
		InSorted.SetNum(InPoints.Num());
		for (int i = 0; i < InSorted.Num(); i++) { InSorted[i] = i; }

		InSorted.Sort(
			[&](const int32 A, const int32 B)
			{
				const FVector V = InPoints[A].Transform.GetLocation() - InPoints[B].Transform.GetLocation();
				return FMath::IsNearlyZero(V.X) ? FMath::IsNearlyZero(V.Y) ? V.Z > 0 : V.Y > 0 : V.X > 0;
			});

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
