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
	}

	void FSubGraph::Invalidate(FGraph* InGraph)
	{
		for (const int32 EdgeIndex : Edges) { InGraph->Edges[EdgeIndex].bValid = false; }
		for (const int32 NodeIndex : Nodes) { InGraph->Nodes[NodeIndex].bValid = false; }
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

#define PCGEX_EDGE_INSERT\
	if (!E.bValid) { continue; } const uint64 Hash = E.H64U(); if (UniqueEdges.Contains(Hash)) { continue; }\
	UniqueEdges.Add(Hash); const FIndexedEdge& Edge = Edges.Emplace_GetRef(Edges.Num(), E.Start, E.End);\
	Nodes[E.Start].Add(Edge.EdgeIndex);	Nodes[E.End].Add(Edge.EdgeIndex);

	void FGraph::InsertEdges(const TArray<FUnsignedEdge>& InEdges)
	{
		FWriteScopeLock WriteLock(GraphLock);
		for (const FUnsignedEdge& E : InEdges) { PCGEX_EDGE_INSERT }
	}

	void FGraph::InsertEdges(const TArray<FIndexedEdge>& InEdges)
	{
		FWriteScopeLock WriteLock(GraphLock);
		for (const FIndexedEdge& E : InEdges) { PCGEX_EDGE_INSERT }
	}

#undef PCGEX_EDGE_INSERT

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

				for (int32 E : Node.Edges)
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

			if (!FMath::IsWithin(SubGraph->Edges.Num(), Min, Max))
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

	void FGraphBuilder::Compile(FPCGExPointsProcessorContext* InContext) const
	{
		InContext->GetAsyncManager()->Start<FPCGExCompileGraphTask>(
			-1, PointIO, const_cast<FGraphBuilder*>(this),
			OutputSettings->GetMinClusterSize(), OutputSettings->GetMaxClusterSize());
	}

	void FGraphBuilder::Write(FPCGExPointsProcessorContext* InContext) const
	{
		EdgesIO->OutputTo(InContext);
	}
}

bool PCGExGraph::FLooseNode::Add(FLooseNode* OtherNode)
{
	if (OtherNode->Index == Index) { return false; }
	if (Neighbors.Contains(OtherNode->Index)) { return true; }

	Neighbors.Add(OtherNode->Index);
	OtherNode->Add(this);
	return true;
}

void PCGExGraph::FLooseNode::AddPointH(const uint64 Point)
{
	FusedPoints.AddUnique(Point);
}

void PCGExGraph::FLooseNode::AddEdgeH(const uint64 Edge)
{
	FusedEdges.AddUnique(Edge);
}

FVector PCGExGraph::FLooseNode::UpdateCenter(PCGExData::FPointIOGroup* IOGroup)
{
	Center = FVector::ZeroVector;
	double Divider = 0;

	uint32 IOIndex = 0;
	uint32 PointIndex = 0;

	for (const uint64 FuseHash : FusedPoints)
	{
		Divider++;
		PCGEx::H64(FuseHash, IOIndex, PointIndex);
		Center += IOGroup->Pairs[IOIndex]->GetInPoint(PointIndex).Transform.GetLocation();
	}

	Center /= Divider;
	return Center;
}

PCGExGraph::FLooseNode* PCGExGraph::FLooseGraph::GetOrCreateNode(const FVector& Position, const int32 IOIndex, const int32 PointIndex)
{
	for (FLooseNode* Node : Nodes) { if ((Position - Node->Center).IsNearlyZero(Tolerance)) { return Node; } }

	FLooseNode* NewNode = new FLooseNode(Position, Nodes.Num());
	NewNode->AddPointH(PCGEx::H64(IOIndex, PointIndex));
	Nodes.Add_GetRef(NewNode);
	return NewNode;
}

void PCGExGraph::FLooseGraph::CreateBridge(const FVector& From, const int32 FromIOIndex, const int32 FromPointIndex, const FVector& To, const int32 ToIOIndex, const int32 ToPointIndex)
{
	FLooseNode* StartVtx = GetOrCreateNode(From, FromIOIndex, FromPointIndex);
	FLooseNode* EndVtx = GetOrCreateNode(To, ToIOIndex, ToPointIndex);
	StartVtx->Add(EndVtx);
	EndVtx->Add(StartVtx);
}

void PCGExGraph::FLooseGraph::GetUniqueEdges(TArray<FUnsignedEdge>& OutEdges)
{
	OutEdges.Empty(Nodes.Num() * 4);
	TSet<uint64> UniqueEdges;
	for (const FLooseNode* Node : Nodes)
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

uint32 PCGExGraph::FEdgePointIntersection::GetTime(const FVector& Position) const
{
	const FVector ClosestPoint = FMath::ClosestPointOnSegment(Position, Start, End);

	if ((ClosestPoint - Position).IsNearlyZero()) { return 0; }                         // Overlap endpoint
	if (FVector::DistSquared(ClosestPoint, Position) >= ToleranceSquared) { return 0; } // Too far

	return (FVector::DistSquared(Start, ClosestPoint) / LengthSquared) * TNumericLimits<uint32>::Max();
}

PCGExGraph::FEdgePointIntersectionList::FEdgePointIntersectionList(
	FGraph* InGraph,
	PCGExData::FPointIO* InPointIO,
	const double Tolerance)
	: PointIO(InPointIO), Graph(InGraph)
{
	const TArray<FPCGPoint>& Points = InPointIO->GetOutIn()->GetPoints();

	const int32 NumEdges = InGraph->Edges.Num();
	Intersections.SetNum(NumEdges);

	for (const FIndexedEdge& Edge : InGraph->Edges)
	{
		if (!Edge.bValid) { continue; }
		Intersections[Edge.EdgeIndex].Init(
			Edge.EdgeIndex,
			Points[Edge.Start].Transform.GetLocation(),
			Points[Edge.End].Transform.GetLocation(),
			Tolerance);
	}
}

void PCGExGraph::FEdgePointIntersectionList::FindIntersections(FPCGExPointsProcessorContext* InContext)
{
	for (const FIndexedEdge& Edge : Graph->Edges)
	{
		if (!Edge.bValid) { continue; }
		InContext->GetAsyncManager()->Start<FPCGExFindPointEdgeIntersectionsTask>(Edge.EdgeIndex, PointIO, this);
	}
}

void PCGExGraph::FEdgePointIntersectionList::Add(const uint64 Intersection, const int32 EdgeIndex)
{
	FWriteScopeLock WriteLock(InsertionLock);
	Intersections[EdgeIndex].CollinearPoints.AddUnique(Intersection);
}

void PCGExGraph::FEdgePointIntersectionList::Insert()
{
	FIndexedEdge NewEdge = FIndexedEdge{};

	for (FEdgePointIntersection& Intersect : Intersections)
	{
		{
			FReadScopeLock ReadLock(InsertionLock);
			if (Intersect.CollinearPoints.IsEmpty()) { continue; }
		}

		FIndexedEdge& SplitEdge = Graph->Edges[Intersect.EdgeIndex];
		SplitEdge.bValid = false; // Invalidate existing edge
		Intersect.CollinearPoints.Sort(
			[](const uint64& A, const uint64& B)
			{
				uint32 A1;
				uint32 A2;
				uint32 B1;
				uint32 B2;
				PCGEx::H64(A, A1, A2);
				PCGEx::H64(B, B1, B2);
				return A2 < B2;
			});

		const int32 FirstIndex = SplitEdge.Start;
		const int32 LastIndex = SplitEdge.End;

		uint32 IOIndex = 0;
		uint32 NodeIndex = 0;

		int32 PrevIndex = FirstIndex;
		for (const uint64 Split : Intersect.CollinearPoints)
		{
			PCGEx::H64(Split, IOIndex, NodeIndex);

			//TODO : Cache local result and batch-add for some perf gain?
			Graph->InsertEdge(PrevIndex, NodeIndex, NewEdge);
			PrevIndex = NodeIndex;
		}

		Graph->InsertEdge(NodeIndex, LastIndex, NewEdge); // Insert last edge
	}
}

void PCGExGraph::FindEdgeIntersections(
	FEdgePointIntersectionList* InList,
	const int32 EdgeIndex,
	const TArray<FPCGPoint>& Points)
{
	const FEdgePointIntersection& Edge = InList->Intersections[EdgeIndex];
	const FIndexedEdge& IEdge = InList->Graph->Edges[EdgeIndex];
	for (const FNode& Node : InList->Graph->Nodes)
	{
		if (!Node.bValid) { continue; }

		FVector Position = Points[Node.PointIndex].Transform.GetLocation();

		if (!Edge.Box.IsInside(Position)) { continue; }
		if (IEdge.Start == Node.PointIndex || IEdge.End == Node.PointIndex) { continue; }

		if (const uint32 Time = Edge.GetTime(Position); Time == 0)
		{
			InList->Add(PCGEx::H64(Node.NodeIndex, Time), EdgeIndex);
		}
	}
}

bool FPCGExFindPointEdgeIntersectionsTask::ExecuteTask()
{
	FindEdgeIntersections(IntersectionList, TaskIndex, PointIO->GetOutIn()->GetPoints());
	return true;
}

bool FPCGExInsertPointEdgeIntersectionsTask::ExecuteTask()
{
	IntersectionList->Insert();
	return true;
}

bool FPCGExWriteSubGraphEdgesTask::ExecuteTask()
{
	TArray<FPCGPoint>& MutablePoints = EdgeIO->GetOut()->GetMutablePoints();
	MutablePoints.SetNum(SubGraph->Edges.Num());

	EdgeIO->CreateOutKeys();

	PCGEx::TFAttributeWriter<int32>* EdgeStart = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::Tag_EdgeStart, -1, false);
	PCGEx::TFAttributeWriter<int32>* EdgeEnd = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::Tag_EdgeEnd, -1, false);

	EdgeStart->BindAndGet(*EdgeIO);
	EdgeEnd->BindAndGet(*EdgeIO);

	int32 PointIndex = 0;

	const TArray<FPCGPoint> Vertices = PointIO->GetOut()->GetPoints();

	for (const int32 EdgeIndex : SubGraph->Edges)
	{
		const PCGExGraph::FIndexedEdge& Edge = Graph->Edges[EdgeIndex];

		if (const FPCGPoint* InEdgePtr = EdgeIO->TryGetInPoint(Edge.PointIndex))
		{
			MutablePoints[PointIndex] = *InEdgePtr; // Copy input edge point if it exists
		}

		FPCGPoint& Point = MutablePoints[PointIndex];
		if (Graph->bWriteEdgePosition)
		{
			Point.Transform.SetLocation(
				FMath::Lerp(
					Vertices[(EdgeStart->Values[PointIndex] = Graph->Nodes[Edge.Start].PointIndex)].Transform.GetLocation(),
					Vertices[(EdgeEnd->Values[PointIndex] = Graph->Nodes[Edge.End].PointIndex)].Transform.GetLocation(), Graph->EdgePosition));
		}

		if (Point.Seed == 0) { PCGExMath::RandomizeSeed(Point); }
		PointIndex++;
	}

	EdgeStart->Write();
	EdgeEnd->Write();

	PCGEX_DELETE(EdgeStart)
	PCGEX_DELETE(EdgeEnd)

	return true;
}

bool FPCGExCompileGraphTask::ExecuteTask()
{
	Builder->Graph->BuildSubGraphs(Min, Max);

	if (Builder->Graph->SubGraphs.IsEmpty())
	{
		Builder->bCompiledSuccessfully = false;
		return false;
	}

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

			for (PCGExGraph::FNode& Node : Builder->Graph->Nodes)
			{
				if (!Node.bValid) { continue; }
				Node.PointIndex = PrunedPoints.Add(MutablePoints[Node.NodeIndex]);
			}

			PointIO->GetOut()->SetPoints(PrunedPoints);
		}
		else
		{
			const int32 NumMaxNodes = Builder->Graph->Nodes.Num();
			MutablePoints.Reserve(NumMaxNodes);

			for (PCGExGraph::FNode& Node : Builder->Graph->Nodes)
			{
				if (!Node.bValid) { continue; }
				Node.PointIndex = MutablePoints.Add(PointIO->GetInPoint(Node.NodeIndex));
			}
		}
	}

	// Cache point index uses by edges
	PCGEx::TFAttributeWriter<int32>* IndexWriter = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::Tag_EdgeIndex, -1, false);
	PCGEx::TFAttributeWriter<int32>* NumEdgesWriter = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::Tag_EdgesNum, 0, false);

	IndexWriter->BindAndGet(*PointIO);
	NumEdgesWriter->BindAndGet(*PointIO);

	for (int i = 0; i < IndexWriter->Values.Num(); i++) { IndexWriter->Values[i] = i; }
	for (const PCGExGraph::FNode& Node : Builder->Graph->Nodes) { if (Node.bValid) { NumEdgesWriter->Values[Node.PointIndex] = Node.NumExportedEdges; } }

	IndexWriter->Write();
	NumEdgesWriter->Write();

	PCGEX_DELETE(IndexWriter)
	PCGEX_DELETE(NumEdgesWriter)

	Builder->bCompiledSuccessfully = true;

	for (PCGExGraph::FSubGraph* SubGraph : Builder->Graph->SubGraphs)
	{
		PCGExData::FPointIO& EdgeIO = Builder->SourceEdgesIO ?
			                              Builder->EdgesIO->Emplace_GetRef(*Builder->SourceEdgesIO, PCGExData::EInit::NewOutput) :
			                              Builder->EdgesIO->Emplace_GetRef(PCGExData::EInit::NewOutput);
		EdgeIO.Tags->Set(PCGExGraph::Tag_Cluster, Builder->EdgeTagValue);
		Manager->Start<FPCGExWriteSubGraphEdgesTask>(-1, PointIO, &EdgeIO, Builder->Graph, SubGraph);
	}

	return true;
}

bool FPCGExUpdateLooseNodeCentersTask::ExecuteTask()
{
	// TODO: Look into plugging blending here?
	for (PCGExGraph::FLooseNode* Node : Graph->Nodes) { Node->UpdateCenter(IOGroup); }

	return true;
}
