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
}

namespace PCGExGraphTask
{
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

		PointIO->CleanupKeys(); //Ensure fresh keys later on

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
}
