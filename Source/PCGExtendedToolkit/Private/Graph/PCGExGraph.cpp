// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExGraph.h"

#include "PCGExPointsProcessor.h"
#include "PCGExRandom.h"


#include "Graph/PCGExCluster.h"
#include "Graph/Data/PCGExClusterData.h"

bool FPCGExGraphBuilderDetails::IsValid(const TSharedPtr<PCGExGraph::FSubGraph>& InSubgraph) const
{
	if (bRemoveBigClusters)
	{
		if (InSubgraph->Edges.Num() > MaxEdgeCount || InSubgraph->Nodes.Num() > MaxVtxCount) { return false; }
	}

	if (bRemoveSmallClusters)
	{
		if (InSubgraph->Edges.Num() < MinEdgeCount || InSubgraph->Nodes.Num() < MinVtxCount) { return false; }
	}

	return true;
}

namespace PCGExGraph
{
	void FSubGraph::Invalidate(FGraph* InGraph)
	{
		for (const int32 EdgeIndex : Edges) { InGraph->Edges[EdgeIndex].bValid = false; }
		for (const int32 NodeIndex : Nodes) { InGraph->Nodes[NodeIndex].bValid = false; }
	}

	TSharedPtr<PCGExCluster::FCluster> FSubGraph::CreateCluster(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) const
	{
		TSharedPtr<PCGExCluster::FCluster> NewCluster = MakeShared<PCGExCluster::FCluster>(VtxDataFacade->Source, EdgesDataFacade->Source);
		NewCluster->BuildFrom(this);

		// Look into the cost of this

		//if (AsyncManager) { NewCluster->ExpandEdges(AsyncManager); }
		//else { NewCluster->GetExpandedEdges(true); }

		return NewCluster;
	}

	int32 FSubGraph::GetFirstInIOIndex()
	{
		for (const int32 InIOIndex : EdgesInIOIndices) { return InIOIndex; }
		return -1;
	}

	void FGraph::ReserveForEdges(const int32 UpcomingAdditionCount)
	{
		const int32 NewMax = Edges.Num() + UpcomingAdditionCount;
		UniqueEdges.Reserve(NewMax);
		Edges.Reserve(NewMax);
		EdgeMetadata.Reserve(UpcomingAdditionCount);
	}

	bool FGraph::InsertEdgeUnsafe(const int32 A, const int32 B, FIndexedEdge& OutEdge, const int32 IOIndex)
	{
		bool bAlreadyExists;
		const uint64 Hash = PCGEx::H64U(A, B);

		UniqueEdges.Add(Hash, &bAlreadyExists);

		if (bAlreadyExists) { return false; }

		OutEdge = Edges.Emplace_GetRef(Edges.Num(), A, B, -1, IOIndex);

		Nodes[A].Add(OutEdge.EdgeIndex);
		Nodes[B].Add(OutEdge.EdgeIndex);

		return true;
	}

	bool FGraph::InsertEdge(const int32 A, const int32 B, FIndexedEdge& OutEdge, const int32 IOIndex)
	{
		FWriteScopeLock WriteLock(GraphLock);
		return InsertEdgeUnsafe(A, B, OutEdge, IOIndex);
	}

	bool FGraph::InsertEdgeUnsafe(const FIndexedEdge& Edge)
	{
		bool bAlreadyExists;
		const uint64 Hash = Edge.H64U();

		UniqueEdges.Add(Hash, &bAlreadyExists);

		if (bAlreadyExists) { return false; }

		UniqueEdges.Add(Hash);

		FIndexedEdge& NewEdge = Edges.Emplace_GetRef(Edge);
		NewEdge.EdgeIndex = Edges.Num() - 1;

		Nodes[Edge.Start].Add(NewEdge.EdgeIndex);
		Nodes[Edge.End].Add(NewEdge.EdgeIndex);

		return true;
	}

	bool FGraph::InsertEdge(const FIndexedEdge& Edge)
	{
		FWriteScopeLock WriteLock(GraphLock);
		return InsertEdgeUnsafe(Edge);
	}

	void FGraph::InsertEdges(const TArray<uint64>& InEdges, const int32 InIOIndex)
	{
		FWriteScopeLock WriteLock(GraphLock);
		uint32 A;
		uint32 B;
		bool bAlreadyExists;

		for (const uint64& E : InEdges)
		{
			UniqueEdges.Add(E, &bAlreadyExists);
			if (bAlreadyExists) { continue; }

			PCGEx::H64(E, A, B);
			const int32 EdgeIndex = Edges.Emplace(Edges.Num(), A, B);
			Nodes[A].Add(EdgeIndex);
			Nodes[B].Add(EdgeIndex);
			Edges[EdgeIndex].IOIndex = InIOIndex;
		}
	}

	int32 FGraph::InsertEdges(const TArray<FIndexedEdge>& InEdges)
	{
		FWriteScopeLock WriteLock(GraphLock);
		const int32 StartIndex = Edges.Num();
		for (const FIndexedEdge& E : InEdges) { InsertEdgeUnsafe(E); }
		return StartIndex;
	}

	void FGraph::InsertEdgesUnsafe(const TSet<uint64>& InEdges, const int32 InIOIndex)
	{
		uint32 A;
		uint32 B;
		bool bAlreadyExists;
		for (const uint64& E : InEdges)
		{
			UniqueEdges.Add(E, &bAlreadyExists);
			if (bAlreadyExists) { continue; }

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
		InsertEdgesUnsafe(InEdges, InIOIndex);
	}

	TArrayView<FNode> FGraph::AddNodes(const int32 NumNewNodes)
	{
		const int32 StartIndex = Nodes.Num();
		Nodes.SetNum(StartIndex + NumNewNodes);
		for (int i = 0; i < NumNewNodes; i++)
		{
			FNode& Node = Nodes[StartIndex + i];
			Node.NodeIndex = Node.PointIndex = StartIndex + i;
			Node.Adjacency.Reserve(NumEdgesReserve);
		}

		return MakeArrayView(Nodes.GetData() + StartIndex, NumNewNodes);
	}


	void FGraph::BuildSubGraphs(const FPCGExGraphBuilderDetails& Limits)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FGraph::BuildSubGraphs);

		TBitArray<> VisitedNodes;
		TBitArray<> VisitedEdges;
		VisitedNodes.Init(false, Nodes.Num());
		VisitedEdges.Init(false, Edges.Num());

		int32 VisitedNum = 0;

		for (int i = 0; i < Nodes.Num(); i++)
		{
			const FNode& CurrentNode = Nodes[i];

			if (VisitedNodes[i]) { continue; }

			VisitedNodes[i] = true;
			VisitedNum++;

			if (!CurrentNode.bValid || CurrentNode.Adjacency.IsEmpty()) { continue; }

			TSharedPtr<FSubGraph> SubGraph = MakeShared<FSubGraph>();
			SubGraph->ParentGraph = this;

			TArray<int32> Stack;
			Stack.Reserve(Nodes.Num() - VisitedNum);
			Stack.Add(i);

			while (Stack.Num() > 0)
			{
#if PCGEX_ENGINE_VERSION <= 503
				const int32 NextIndex = Stack.Pop(false);
#else
				const int32 NextIndex = Stack.Pop(EAllowShrinking::No);
#endif
				FNode& Node = Nodes[NextIndex];
				Node.NumExportedEdges = 0;

				for (const int32 E : Node.Adjacency)
				{
					if (VisitedEdges[E]) { continue; }

					VisitedEdges[E] = true;

					const FIndexedEdge& Edge = Edges[E];

					if (!Edge.bValid) { continue; }

					const int32 OtherIndex = Edge.Other(NextIndex);
					if (!Nodes[OtherIndex].bValid) { continue; }

					Node.NumExportedEdges++;
					SubGraph->Add(Edge, this);

					if (!VisitedNodes[OtherIndex]) // Only enqueue if not already visited
					{
						VisitedNodes[OtherIndex] = true;
						VisitedNum++;

						Stack.Add(OtherIndex);
					}
				}
			}


			if (!Limits.IsValid(SubGraph)) { SubGraph->Invalidate(this); } // Will invalidate isolated points
			else { SubGraphs.Add(SubGraph); }
		}
	}

	void FGraph::GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth) const
	{
		const int32 NextDepth = SearchDepth - 1;
		const FNode& RootNode = Nodes[FromIndex];

		for (const int32 EdgeIndex : RootNode.Adjacency)
		{
			const FIndexedEdge& Edge = Edges[EdgeIndex];
			if (!Edge.bValid) { continue; }

			int32 OtherIndex = Edge.Other(FromIndex);
			if (OutIndices.Contains(OtherIndex)) { continue; }

			OutIndices.Add(OtherIndex);
			if (NextDepth > 0) { GetConnectedNodes(OtherIndex, OutIndices, NextDepth); }
		}
	}

	void FGraphBuilder::CompileAsync(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager, const bool bWriteNodeFacade, FGraphMetadataDetails* MetadataDetails)
	{
		InAsyncManager->Start<PCGExGraphTask::FCompileGraph>(-1, NodeDataFacade->Source, SharedThis(this), bWriteNodeFacade, MetadataDetails);
	}

	void FGraphBuilder::Compile(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager, const bool bWriteNodeFacade, FGraphMetadataDetails* MetadataDetails)
	{
		AsyncManager = InAsyncManager;
		MetadataDetailsPtr = MetadataDetails;
		bWriteVtxDataFacadeWithCompile = bWriteNodeFacade;

		TRACE_CPUPROFILER_EVENT_SCOPE(FGraphBuilder::Compile);

		Graph->BuildSubGraphs(*OutputDetails);

		if (Graph->SubGraphs.IsEmpty())
		{
			bCompiledSuccessfully = false;
			if (OnCompilationEndCallback)
			{
				const TSharedPtr<FGraphBuilder> SharedPtr = SharedThis(this);
				OnCompilationEndCallback(SharedPtr.ToSharedRef(), bCompiledSuccessfully);
			}
			return;
		}

		NodeDataFacade->Source->CleanupKeys(); //Ensure fresh keys later on

		TArray<FNode>& Nodes = Graph->Nodes;

		TArray<int32> InternalValidNodes;
		TArray<int32>& ValidNodes = InternalValidNodes;
		if (OutputNodeIndices) { ValidNodes = *OutputNodeIndices; }

		ValidNodes.Reserve(Nodes.Num());

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FCompileGraph::PrunePoints);

			UPCGMetadata* OutPointsMetadata = NodeDataFacade->Source->GetOut()->Metadata;

			// Rebuild point list with only the one used
			// to know which are used, we need to prune subgraphs first
			TArray<FPCGPoint>& MutablePoints = NodeDataFacade->GetOut()->GetMutablePoints();

			if (!MutablePoints.IsEmpty())
			{
				//Assume points were filled before, and remove them from the current array
				TArray<FPCGPoint> PrunedPoints;
				PrunedPoints.Reserve(MutablePoints.Num());

				for (FNode& Node : Nodes)
				{
					if (!Node.bValid || Node.Adjacency.IsEmpty()) { continue; }
					Node.PointIndex = PrunedPoints.Add(MutablePoints[Node.PointIndex]);
					ValidNodes.Add(Node.NodeIndex);
				}

				NodeDataFacade->GetOut()->SetPoints(PrunedPoints);
				TArray<FPCGPoint>& ValidPoints = NodeDataFacade->GetOut()->GetMutablePoints();
				for (FPCGPoint& ValidPoint : ValidPoints) { OutPointsMetadata->InitializeOnSet(ValidPoint.MetadataEntry); }
			}
			else
			{
				const int32 NumMaxNodes = Nodes.Num();
				MutablePoints.Reserve(NumMaxNodes);

				const TArray<FPCGPoint> OriginalPoints = NodeDataFacade->GetIn()->GetPoints();

				for (FNode& Node : Nodes)
				{
					if (!Node.bValid || Node.Adjacency.IsEmpty()) { continue; }
					Node.PointIndex = MutablePoints.Add(OriginalPoints[Node.PointIndex]);
					OutPointsMetadata->InitializeOnSet(MutablePoints.Last().MetadataEntry);
					ValidNodes.Add(Node.NodeIndex);
				}
			}
		}

		const TSharedPtr<PCGExData::TBuffer<int64>> VtxEndpointWriter = NodeDataFacade->GetWritable<int64>(Tag_VtxEndpoint, 0, false, true);

		const uint32 BaseGUID = NodeDataFacade->GetOut()->GetUniqueID();
		for (const int32 NodeIndex : ValidNodes)
		{
			const FNode& Node = Nodes[NodeIndex];
			VtxEndpointWriter->GetMutable(Node.PointIndex) = PCGEx::H64(NodeGUID(BaseGUID, Node.PointIndex), Node.NumExportedEdges);
		}

		if (MetadataDetails && !Graph->NodeMetadata.IsEmpty())
		{
#define PCGEX_FOREACH_NODE_METADATA(MACRO)\
			MACRO(IsPointUnion, bool, false, IsUnion()) \
			MACRO(PointUnionSize, int32, 0, UnionSize) \
			MACRO(IsIntersector, bool, false, IsIntersector()) \
			MACRO(Crossing, bool, false, IsCrossing())
#define PCGEX_NODE_METADATA_DECL(_NAME, _TYPE, _DEFAULT, _ACCESSOR) const TSharedPtr<PCGExData::TBuffer<_TYPE>> _NAME##Buffer = MetadataDetails->bWrite##_NAME ? NodeDataFacade->GetWritable<_TYPE>(MetadataDetails->_NAME##AttributeName, _DEFAULT, true, true) : nullptr;
#define PCGEX_NODE_METADATA_OUTPUT(_NAME, _TYPE, _DEFAULT, _ACCESSOR) if(_NAME##Buffer){_NAME##Buffer->GetMutable(PointIndex) = NodeMeta->_ACCESSOR;}

			PCGEX_FOREACH_NODE_METADATA(PCGEX_NODE_METADATA_DECL)

			for (const int32 NodeIndex : ValidNodes)
			{
				const FGraphNodeMetadata* NodeMeta = Graph->FindNodeMetadata(NodeIndex);

				if (!NodeMeta) { continue; }

				const int32 PointIndex = Nodes[NodeIndex].PointIndex;
				PCGEX_FOREACH_NODE_METADATA(PCGEX_NODE_METADATA_OUTPUT)
			}

#undef PCGEX_FOREACH_NODE_METADATA
#undef PCGEX_NODE_METADATA_DECL
#undef PCGEX_NODE_METADATA_OUTPUT
		}

		bCompiledSuccessfully = true;

		// Subgraphs

		for (int i = 0; i < Graph->SubGraphs.Num(); i++)
		{
			const TSharedPtr<FSubGraph>& SubGraph = Graph->SubGraphs[i];
			TSharedPtr<PCGExData::FPointIO> EdgeIO;

			if (const int32 IOIndex = SubGraph->GetFirstInIOIndex(); SourceEdgesIO && SourceEdgesIO->Pairs.IsValidIndex(IOIndex))
			{
				EdgeIO = EdgesIO->Emplace_GetRef<UPCGExClusterEdgesData>(SourceEdgesIO->Pairs[IOIndex], PCGExData::EInit::NewOutput);
			}
			else
			{
				EdgeIO = EdgesIO->Emplace_GetRef<UPCGExClusterEdgesData>(PCGExData::EInit::NewOutput);
			}

			SubGraph->UID = EdgeIO->GetOut()->GetUniqueID();

			SubGraph->VtxDataFacade = NodeDataFacade;
			SubGraph->EdgesDataFacade = MakeShared<PCGExData::FFacade>(EdgeIO.ToSharedRef());

			MarkClusterEdges(EdgeIO, PairIdStr);
		}

		MarkClusterVtx(NodeDataFacade->Source, PairIdStr);

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ProcessSubGraphTask)
		ProcessSubGraphTask->OnCompleteCallback =
			[WeakThis = TWeakPtr<FGraphBuilder>(SharedThis(this))]()
			{
				if (const TSharedPtr<FGraphBuilder> Builder = WeakThis.Pin())
				{
					if (Builder->OnCompilationEndCallback) { Builder->OnCompilationEndCallback(Builder.ToSharedRef(), Builder->bCompiledSuccessfully); }
					if (!Builder->bCompiledSuccessfully) { return; }

					if (Builder->bWriteVtxDataFacadeWithCompile) { Builder->NodeDataFacade->Write(Builder->AsyncManager); }
				}
			};

		ProcessSubGraphTask->OnIterationCallback =
			[WeakThis = TWeakPtr<FGraphBuilder>(SharedThis(this))](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				if (const TSharedPtr<FGraphBuilder> Builder = WeakThis.Pin())
				{
					const TSharedPtr<FSubGraph> SubGraph = Builder->Graph->SubGraphs[Index];
					PCGExGraphTask::WriteSubGraphEdges(Builder->AsyncManager, SubGraph, Builder->MetadataDetailsPtr);
				}
			};

		ProcessSubGraphTask->StartIterations(Graph->SubGraphs.Num(), 1, false, false);
	}

	void FGraphBuilder::StageEdgesOutputs() const
	{
		EdgesIO->StageOutputs();
	}
}

namespace PCGExGraphTask
{
	void WriteSubGraphEdges(
		const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager,
		const TSharedPtr<PCGExGraph::FSubGraph>& SubGraph,
		const PCGExGraph::FGraphMetadataDetails* MetadataDetails)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FWriteSubGraphEdges::ExecuteTask);

		const TSharedPtr<PCGExData::FFacade> VtxDataFacade = SubGraph->VtxDataFacade;

		const TArray<FPCGPoint>& Vertices = VtxDataFacade->GetOut()->GetPoints();

		PCGExGraph::FGraph* Graph = SubGraph->ParentGraph;
		TArray<int32> EdgeDump = SubGraph->Edges.Array();
		const int32 NumEdges = EdgeDump.Num();

		TArray<int32> RootEdgeIndices;
		RootEdgeIndices.SetNumUninitialized(NumEdges);

		TArray<PCGExGraph::FIndexedEdge>& FlattenedEdges = SubGraph->FlattenedEdges;
		PCGEx::InitArray(FlattenedEdges, NumEdges);

		const TSharedPtr<PCGExData::FPointIO> EdgeIO = SubGraph->EdgesDataFacade->Source;

		WriteMark(EdgeIO.ToSharedRef(), PCGExGraph::Tag_ClusterId, SubGraph->UID);

		TArray<FPCGPoint>& MutablePoints = EdgeIO->GetOut()->GetMutablePoints();
		MutablePoints.SetNum(NumEdges);

		if (EdgeIO->GetIn())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FWriteSubGraphEdges::GatherPreExistingPoints);

			// Copy any existing point properties first
			UPCGMetadata* Metadata = EdgeIO->GetOut()->Metadata;
			const TArray<FPCGPoint>& InPoints = EdgeIO->GetIn()->GetPoints();
			for (int i = 0; i < NumEdges; i++)
			{
				const PCGExGraph::FIndexedEdge& OE = Graph->Edges[EdgeDump[i]];
				FlattenedEdges[i] = PCGExGraph::FIndexedEdge(i, Graph->Nodes[OE.Start].PointIndex, Graph->Nodes[OE.End].PointIndex, i);
				RootEdgeIndices[i] = OE.EdgeIndex;
				if (InPoints.IsValidIndex(OE.PointIndex)) { MutablePoints[i] = InPoints[OE.PointIndex]; }
				Metadata->InitializeOnSet(MutablePoints[i].MetadataEntry);
			}
		}
		else
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FWriteSubGraphEdges::CreatePoints);

			UPCGMetadata* Metadata = EdgeIO->GetOut()->Metadata;

			for (int i = 0; i < NumEdges; i++)
			{
				const PCGExGraph::FIndexedEdge& E = Graph->Edges[EdgeDump[i]];
				FlattenedEdges[i] = PCGExGraph::FIndexedEdge(i, Graph->Nodes[E.Start].PointIndex, Graph->Nodes[E.End].PointIndex, i);
				RootEdgeIndices[i] = E.EdgeIndex;
				Metadata->InitializeOnSet(MutablePoints[i].MetadataEntry);
			}
		}

		const TSharedPtr<PCGExData::TBuffer<int32>> NumClusterIdWriter = VtxDataFacade->GetWritable<int32>(PCGExGraph::Tag_ClusterId, -1, false, true);
		const TSharedPtr<PCGExData::TBuffer<int64>> EdgeEndpointsWriter = SubGraph->EdgesDataFacade->GetWritable<int64>(PCGExGraph::Tag_EdgeEndpoints, -1, false, true);

		const FVector SeedOffset = FVector(EdgeIO->IOIndex);

		const int32 ClusterId = SubGraph->UID;
		const uint32 BaseGUID = VtxDataFacade->Source->GetOut()->GetUniqueID();

		for (const PCGExGraph::FIndexedEdge& E : FlattenedEdges)
		{
			FPCGPoint& EdgePt = MutablePoints[E.EdgeIndex];

			// Mark matching Vtx
			NumClusterIdWriter->GetMutable(E.Start) = ClusterId;
			NumClusterIdWriter->GetMutable(E.End) = ClusterId;

			EdgeEndpointsWriter->GetMutable(E.EdgeIndex) = PCGEx::H64(PCGExGraph::NodeGUID(BaseGUID, E.Start), PCGExGraph::NodeGUID(BaseGUID, E.End));

			if (Graph->bWriteEdgePosition)
			{
				EdgePt.Transform.SetLocation(
					FMath::Lerp(
						Vertices[E.Start].Transform.GetLocation(),
						Vertices[E.End].Transform.GetLocation(),
						Graph->EdgePosition));
			}

			if (EdgePt.Seed == 0 || Graph->bRefreshEdgeSeed) { EdgePt.Seed = PCGExRandom::ComputeSeed(EdgePt, SeedOffset); }
		}

		if (MetadataDetails && !Graph->EdgeMetadata.IsEmpty())
		{
			// Note : better move & write this in the UnionHelper, we'll inherit properties during edge/edge intersections that way.
			// Less edges to manage, even tho more attributes.
#define PCGEX_FOREACH_EDGE_METADATA(MACRO)\
MACRO(IsEdgeUnion, bool, false, IsUnion()) \
MACRO(EdgeUnionSize, int32, 0, UnionSize)
#define PCGEX_EDGE_METADATA_DECL(_NAME, _TYPE, _DEFAULT, _ACCESSOR) const TSharedPtr<PCGExData::TBuffer<_TYPE>> _NAME##Buffer = MetadataDetails->bWrite##_NAME ? SubGraph->EdgesDataFacade->GetWritable<_TYPE>(MetadataDetails->_NAME##AttributeName, _DEFAULT, true, true) : nullptr;
#define PCGEX_EDGE_METADATA_OUTPUT(_NAME, _TYPE, _DEFAULT, _ACCESSOR) if(_NAME##Buffer){_NAME##Buffer->GetMutable(PointIndex) = EdgeMeta->_ACCESSOR;}

			PCGEX_FOREACH_EDGE_METADATA(PCGEX_EDGE_METADATA_DECL)

			for (const PCGExGraph::FIndexedEdge& E : FlattenedEdges)
			{
				const int32 PointIndex = E.EdgeIndex;
				if (const PCGExGraph::FGraphEdgeMetadata* EdgeMeta = Graph->FindRootEdgeMetadata(RootEdgeIndices[E.EdgeIndex]))
				{
					PCGEX_FOREACH_EDGE_METADATA(PCGEX_EDGE_METADATA_OUTPUT)
				}
			}

#undef PCGEX_FOREACH_EDGE_METADATA
#undef PCGEX_EDGE_METADATA_DECL
#undef PCGEX_EDGE_METADATA_OUTPUT
		}

		if (GetDefault<UPCGExGlobalSettings>()->bCacheClusters && Graph->bBuildClusters)
		{
			if (UPCGExClusterEdgesData* ClusterEdgesData = Cast<UPCGExClusterEdgesData>(EdgeIO->GetOut()))
			{
				AsyncManager->Start<FWriteSubGraphCluster>(-1, nullptr, SubGraph);
			}
		}

		SubGraph->EdgesDataFacade->Write(AsyncManager);
	}

	bool FWriteSubGraphCluster::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		UPCGExClusterEdgesData* ClusterEdgesData = Cast<UPCGExClusterEdgesData>(SubGraph->EdgesDataFacade->GetOut());
		const TSharedPtr<PCGExCluster::FCluster> Cluster = SubGraph->CreateCluster(AsyncManager);
		if (SubGraph->ParentGraph->bExpandClusters) { Cluster->ExpandNodes(AsyncManager); }

		ClusterEdgesData->SetBoundCluster(Cluster);
		return true;
	}


	bool FCompileGraph::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCompileGraph::ExecuteTask);

		Builder->Compile(AsyncManager, bWriteNodeFacade, MetadataDetails);
		return true;
	}

	bool FCopyGraphToPoint::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		if (!GraphBuilder->bCompiledSuccessfully) { return false; }

		const TSharedPtr<PCGExData::FPointIO> VtxDupe = VtxCollection->Emplace_GetRef(GraphBuilder->NodeDataFacade->GetOut(), PCGExData::EInit::DuplicateInput);
		VtxDupe->IOIndex = TaskIndex;

		FString OutId;
		PCGExGraph::SetClusterVtx(VtxDupe, OutId);

		InternalStart<PCGExGeoTasks::FTransformPointIO>(TaskIndex, PointIO, VtxDupe, TransformDetails);

		for (const TSharedPtr<PCGExData::FPointIO>& Edges : GraphBuilder->EdgesIO->Pairs)
		{
			TSharedPtr<PCGExData::FPointIO> EdgeDupe = EdgeCollection->Emplace_GetRef(Edges->GetOut(), PCGExData::EInit::DuplicateInput);
			EdgeDupe->IOIndex = TaskIndex;
			PCGExGraph::MarkClusterEdges(EdgeDupe, OutId);

			InternalStart<PCGExGeoTasks::FTransformPointIO>(TaskIndex, PointIO, EdgeDupe, TransformDetails);
		}

		// TODO : Copy & Transform cluster as well for a big perf boost

		return true;
	}
}
