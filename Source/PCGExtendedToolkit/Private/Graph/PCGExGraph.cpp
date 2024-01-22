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
		const uint64 Hash = GetUnsignedHash64(A, B);

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
		const uint64 Hash = Edge.GetUnsignedHash();

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
	if (!E.bValid) { continue; } const uint64 Hash = E.GetUnsignedHash(); if (UniqueEdges.Contains(Hash)) { continue; }\
	UniqueEdges.Add(Hash); const FIndexedEdge& Edge = Edges.Emplace_GetRef(Edges.Num(), E.Start, E.End);\
	Nodes[E.Start].Add(Edge.EdgeIndex);	Nodes[E.End].Add(Edge.EdgeIndex);

	void FGraph::InsertEdges(const TArray<FUnsignedEdge>& InEdges)
	{
		for (const FUnsignedEdge& E : InEdges) { PCGEX_EDGE_INSERT }
	}

	void FGraph::InsertEdges(const TArray<FIndexedEdge>& InEdges)
	{
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

		if (Point.Seed == 0) { PCGEx::RandomizeSeed(Point); }
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

	if (Builder->Graph->SubGraphs.IsEmpty())
	{
		Builder->bCompiledSuccessfully = false;
		return false;
	}

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
