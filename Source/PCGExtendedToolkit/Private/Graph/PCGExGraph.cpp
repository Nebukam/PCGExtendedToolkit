// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExGraph.h"

#include "PCGExPointsProcessor.h"
#include "PCGExRandom.h"
#include "Data/Blending/PCGExUnionBlender.h"


#include "Graph/PCGExCluster.h"
#include "Graph/Data/PCGExClusterData.h"

void FPCGExBasicEdgeSolidificationDetails::Mutate(FPCGPoint& InEdgePoint, const FPCGPoint& InStart, const FPCGPoint& InEnd, const double InLerp) const
{
	const FVector A = InStart.Transform.GetLocation();
	const FVector B = InEnd.Transform.GetLocation();


	InEdgePoint.Transform.SetLocation(FMath::Lerp(A, B, InLerp));
	if (SolidificationAxis == EPCGExMinimalAxis::None) { return; }

	const FVector EdgeDirection = (A - B).GetSafeNormal();

	const double EdgeLength = FVector::Dist(A, B);
	const double StartRadius = InStart.GetScaledExtents().Size();
	const double EndRadius = InStart.GetScaledExtents().Size();

	double Rad = 0;

	switch (RadiusType)
	{
	case EPCGExBasicEdgeRadius::Average:
		Rad = (StartRadius + EndRadius) * 0.5 * RadiusScale;
		break;
	case EPCGExBasicEdgeRadius::Lerp:
		Rad = FMath::Lerp(StartRadius, EndRadius, InLerp) * RadiusScale;
		break;
	case EPCGExBasicEdgeRadius::Min:
		Rad = FMath::Min(StartRadius, EndRadius) * RadiusScale;
		break;
	case EPCGExBasicEdgeRadius::Max:
		Rad = FMath::Max(StartRadius, EndRadius) * RadiusScale;
		break;
	default:
	case EPCGExBasicEdgeRadius::Fixed:
		Rad = RadiusConstant;
		break;
	}

	FRotator EdgeRot;
	FVector BoundsMin = FVector(-Rad);
	FVector BoundsMax = FVector(Rad);

	const FVector PtScale = InEdgePoint.Transform.GetScale3D();
	const FVector InvScale = FVector::One() / PtScale;

	const double LerpInv = 1 - InLerp;
	bool bProcessAxis;

#define PCGEX_SOLIDIFY_DIMENSION(_AXIS)\
	bProcessAxis = SolidificationAxis == EPCGExMinimalAxis::_AXIS;\
	if (bProcessAxis){\
		if (SolidificationAxis == EPCGExMinimalAxis::_AXIS){ BoundsMin._AXIS = (-EdgeLength * LerpInv) * InvScale._AXIS; BoundsMax._AXIS = (EdgeLength * InLerp) * InvScale._AXIS; } \
		else{ BoundsMin._AXIS = (-Rad) * InvScale._AXIS; BoundsMax._AXIS = (Rad) * InvScale._AXIS; }}

	PCGEX_FOREACH_XYZ(PCGEX_SOLIDIFY_DIMENSION)
#undef PCGEX_SOLIDIFY_DIMENSION

	switch (SolidificationAxis)
	{
	default:
	case EPCGExMinimalAxis::X:
		EdgeRot = FRotationMatrix::MakeFromX(EdgeDirection).Rotator();
		break;
	case EPCGExMinimalAxis::Y:
		EdgeRot = FRotationMatrix::MakeFromY(EdgeDirection).Rotator();
		break;
	case EPCGExMinimalAxis::Z:
		EdgeRot = FRotationMatrix::MakeFromZ(EdgeDirection).Rotator();
		break;
	}

	InEdgePoint.Transform = FTransform(EdgeRot, FMath::Lerp(B, A, LerpInv), InEdgePoint.Transform.GetScale3D());

	InEdgePoint.BoundsMin = BoundsMin;
	InEdgePoint.BoundsMax = BoundsMax;
}

bool FPCGExGraphBuilderDetails::WantsClusters() const
{
	PCGEX_GET_OPTION_STATE(BuildAndCacheClusters, bDefaultBuildAndCacheClusters)
}

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

	void FSubGraph::BuildCluster(const TSharedRef<PCGExCluster::FCluster>& InCluster)
	{
		// Correct edge IO Index that has been overwritten during subgraph processing
		for (FEdge& E : FlattenedEdges) { E.IOIndex = -1; }

		InCluster->BuildFrom(SharedThis(this));

		// Look into the cost of this

		//if (AsyncManager) { NewCluster->ExpandEdges(AsyncManager); }
		//else { NewCluster->GetExpandedEdges(true); }
	}

	int32 FSubGraph::GetFirstInIOIndex()
	{
		for (const int32 InIOIndex : EdgesInIOIndices) { return InIOIndex; }
		return -1;
	}

	void FSubGraph::Compile(
		const TWeakPtr<PCGExMT::FAsyncMultiHandle>& InParentHandle,
		const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager,
		const TSharedPtr<FGraphBuilder>& InBuilder)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FWriteSubGraphEdges::ExecuteTask);

		const TSharedPtr<FGraph> ParentGraph = WeakParentGraph.Pin();

		WeakBuilder = InBuilder;
		WeakAsyncManager = AsyncManager;

		TArray<int32> EdgeDump = Edges.Array();
		const int32 NumEdges = EdgeDump.Num();

		PCGEx::InitArray(FlattenedEdges, NumEdges);

		TArray<FPCGPoint>& MutablePoints = EdgesDataFacade->Source->GetOut()->GetMutablePoints();
		MutablePoints.SetNum(NumEdges);

		if (EdgesDataFacade->Source->GetIn())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FWriteSubGraphEdges::GatherPreExistingPoints);

			// Copy any existing point properties first
			UPCGMetadata* Metadata = EdgesDataFacade->Source->GetOut()->Metadata;
			const TArray<FPCGPoint>& InPoints = EdgesDataFacade->Source->GetIn()->GetPoints();
			for (int i = 0; i < NumEdges; i++)
			{
				const FEdge& OE = ParentGraph->Edges[EdgeDump[i]];
				FlattenedEdges[i] = FEdge(i, ParentGraph->Nodes[OE.Start].PointIndex, ParentGraph->Nodes[OE.End].PointIndex, i, OE.Index); // Use flat edge IOIndex to store original edge index
				if (InPoints.IsValidIndex(OE.PointIndex)) { MutablePoints[i] = InPoints[OE.PointIndex]; }
				Metadata->InitializeOnSet(MutablePoints[i].MetadataEntry);
			}
		}
		else
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FWriteSubGraphEdges::CreatePoints);

			UPCGMetadata* Metadata = EdgesDataFacade->Source->GetOut()->Metadata;

			for (int i = 0; i < NumEdges; i++)
			{
				const FEdge& E = ParentGraph->Edges[EdgeDump[i]];
				FlattenedEdges[i] = FEdge(i, ParentGraph->Nodes[E.Start].PointIndex, ParentGraph->Nodes[E.End].PointIndex, i, E.Index);
				Metadata->InitializeOnSet(MutablePoints[i].MetadataEntry);
			}
		}

		MetadataDetails = InBuilder->GetMetadataDetails();
		const bool bHasUnionMetadata = (MetadataDetails && InBuilder && !ParentGraph->EdgeMetadata.IsEmpty());

#define PCGEX_FOREACH_EDGE_METADATA(MACRO)\
MACRO(IsEdgeUnion, bool, false, IsUnion()) \
MACRO(EdgeUnionSize, int32, 0, UnionSize)

#define PCGEX_EDGE_METADATA_DECL(_NAME, _TYPE, _DEFAULT, _ACCESSOR) if(bHasUnionMetadata && MetadataDetails->bWrite##_NAME){ _NAME##Buffer = EdgesDataFacade->GetWritable<_TYPE>(MetadataDetails->_NAME##AttributeName, _DEFAULT, true, PCGExData::EBufferInit::New); }
		PCGEX_FOREACH_EDGE_METADATA(PCGEX_EDGE_METADATA_DECL)
#undef PCGEX_EDGE_METADATA_DECL

		Distances = PCGExDetails::MakeNoneDistances();

		if (InBuilder->SourceEdgeFacades && ParentGraph->EdgesUnion)
		{
			UnionBlender = MakeShared<PCGExDataBlending::FUnionBlender>(MetadataDetails->EdgesBlendingDetailsPtr, MetadataDetails->EdgesCarryOverDetails);
			UnionBlender->AddSources(*InBuilder->SourceEdgeFacades, &ProtectedClusterAttributes);
			UnionBlender->PrepareMerge(AsyncManager->GetContext(), EdgesDataFacade, ParentGraph->EdgesUnion);
		}

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ProcessSubGraphEdges)

		ProcessSubGraphEdges->SetParent(InParentHandle.Pin());

		ProcessSubGraphEdges->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->CompilationComplete();
			};

		ProcessSubGraphEdges->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				This->CompileRange(Scope);
			};

		ProcessSubGraphEdges->StartSubLoops(FlattenedEdges.Num(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}

	void FSubGraph::CompileRange(const PCGExMT::FScope& Scope)
	{
		const TSharedPtr<FGraph> ParentGraph = WeakParentGraph.Pin();
		const TSharedPtr<FGraphBuilder> Builder = WeakBuilder.Pin();

		if (!ParentGraph || !Builder) { return; }

		const TSharedPtr<PCGExData::TBuffer<int64>> EdgeEndpointsWriter = EdgesDataFacade->GetWritable<int64>(Tag_EdgeEndpoints, -1, false, PCGExData::EBufferInit::New);

		const TArray<FPCGPoint>& Vertices = VtxDataFacade->GetOut()->GetPoints();
		TArray<FPCGPoint>& MutablePoints = EdgesDataFacade->Source->GetOut()->GetMutablePoints();

		const bool bHasUnionMetadata = (MetadataDetails && !ParentGraph->EdgeMetadata.IsEmpty());
		const FVector SeedOffset = FVector(EdgesDataFacade->Source->IOIndex);
		const uint32 BaseGUID = VtxDataFacade->Source->GetOut()->GetUniqueID();

		for (int i = Scope.Start; i < Scope.End; i++)
		{
			const FEdge& E = FlattenedEdges[i];
			const int32 EdgeIndex = E.Index;
			FPCGPoint& EdgePt = MutablePoints[EdgeIndex];

			if (bHasUnionMetadata)
			{
				if (const FGraphEdgeMetadata* EdgeMeta = ParentGraph->FindRootEdgeMetadata_Unsafe(E.IOIndex))
				{
					if (UnionBlender)
					{
						UnionBlender->MergeSingle(EdgeIndex, ParentGraph->EdgesUnion->Get(EdgeMeta->RootIndex), Distances);
					}

#define PCGEX_EDGE_METADATA_OUTPUT(_NAME, _TYPE, _DEFAULT, _ACCESSOR) if(_NAME##Buffer){_NAME##Buffer->GetMutable(EdgeIndex) = EdgeMeta->_ACCESSOR;}
					PCGEX_FOREACH_EDGE_METADATA(PCGEX_EDGE_METADATA_OUTPUT)
#undef PCGEX_EDGE_METADATA_OUTPUT
				}
			}

			EdgeEndpointsWriter->GetMutable(EdgeIndex) = PCGEx::H64(NodeGUID(BaseGUID, E.Start), NodeGUID(BaseGUID, E.End));

			if (Builder->OutputDetails->bWriteEdgePosition)
			{
				Builder->OutputDetails->BasicEdgeSolidification.Mutate(EdgePt, Vertices[E.Start], Vertices[E.End], Builder->OutputDetails->EdgePosition);
			}

			if (EdgePt.Seed == 0 || ParentGraph->bRefreshEdgeSeed) { EdgePt.Seed = PCGExRandom::ComputeSeed(EdgePt, SeedOffset); }
		}
	}

	void FSubGraph::CompilationComplete()
	{
		UnionBlender.Reset();

		const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager = WeakAsyncManager.Pin();
		const TSharedPtr<FGraph> ParentGraph = WeakParentGraph.Pin();
		if (!AsyncManager || !AsyncManager->IsAvailable() || !ParentGraph) { return; }

		PCGEX_SHARED_THIS_DECL

		if (GetDefault<UPCGExGlobalSettings>()->bCacheClusters && ParentGraph->bBuildClusters)
		{
			if (Cast<UPCGExClusterEdgesData>(EdgesDataFacade->Source->GetOut()))
			{
				PCGEX_LAUNCH(PCGExGraphTask::FWriteSubGraphCluster, ThisPtr)
			}
		}

		if (OnSubGraphPostProcess) { OnSubGraphPostProcess(ThisPtr.ToSharedRef()); }
		EdgesDataFacade->Write(AsyncManager);
	}

#undef PCGEX_FOREACH_EDGE_METADATA

	void FGraph::ReserveForEdges(const int32 UpcomingAdditionCount)
	{
		const int32 NewMax = Edges.Num() + UpcomingAdditionCount;
		UniqueEdges.Reserve(NewMax);
		Edges.Reserve(NewMax);
		EdgeMetadata.Reserve(UpcomingAdditionCount);
	}

	bool FGraph::InsertEdge_Unsafe(const int32 A, const int32 B, FEdge& OutEdge, const int32 IOIndex)
	{
		check(A != B)

		const uint64 Hash = PCGEx::H64U(A, B);

		if (UniqueEdges.Contains(Hash)) { return false; }

		OutEdge = Edges.Emplace_GetRef(Edges.Num(), A, B, -1, IOIndex);
		UniqueEdges.Add(Hash, (OutEdge.Index = Edges.Num() - 1));

		Nodes[A].LinkEdge(OutEdge.Index);
		Nodes[B].LinkEdge(OutEdge.Index);

		return true;
	}

	bool FGraph::InsertEdge(const int32 A, const int32 B, FEdge& OutEdge, const int32 IOIndex)
	{
		FWriteScopeLock WriteLock(GraphLock);
		return InsertEdge_Unsafe(A, B, OutEdge, IOIndex);
	}

	bool FGraph::InsertEdge_Unsafe(const FEdge& Edge)
	{
		uint64 H = Edge.H64U();
		if (UniqueEdges.Contains(H)) { return false; }

		FEdge& NewEdge = Edges.Emplace_GetRef(Edge);
		UniqueEdges.Add(H, (NewEdge.Index = Edges.Num() - 1));

		Nodes[Edge.Start].LinkEdge(NewEdge.Index);
		Nodes[Edge.End].LinkEdge(NewEdge.Index);

		return true;
	}

	bool FGraph::InsertEdge(const FEdge& Edge)
	{
		FWriteScopeLock WriteLock(GraphLock);
		return InsertEdge_Unsafe(Edge);
	}

	bool FGraph::InsertEdge_Unsafe(const FEdge& Edge, FEdge& OutEdge, const int32 InIOIndex)
	{
		return InsertEdge_Unsafe(Edge.Start, Edge.End, OutEdge, InIOIndex);
	}

	bool FGraph::InsertEdge(const FEdge& Edge, FEdge& OutEdge, const int32 InIOIndex)
	{
		return InsertEdge(Edge.Start, Edge.End, OutEdge, InIOIndex);
	}

	void FGraph::InsertEdges(const TArray<uint64>& InEdges, const int32 InIOIndex)
	{
		FWriteScopeLock WriteLock(GraphLock);
		uint32 A;
		uint32 B;

		for (const uint64& E : InEdges)
		{
			if (UniqueEdges.Contains(E)) { continue; }

			PCGEx::H64(E, A, B);

			check(A != B)

			const int32 EdgeIndex = Edges.Emplace(Edges.Num(), A, B);
			UniqueEdges.Add(E, EdgeIndex);
			Nodes[A].LinkEdge(EdgeIndex);
			Nodes[B].LinkEdge(EdgeIndex);
			Edges[EdgeIndex].IOIndex = InIOIndex;
		}
	}

	int32 FGraph::InsertEdges(const TArray<FEdge>& InEdges)
	{
		FWriteScopeLock WriteLock(GraphLock);
		const int32 StartIndex = Edges.Num();
		for (const FEdge& E : InEdges) { InsertEdge_Unsafe(E); }
		return StartIndex;
	}

	void FGraph::InsertEdges_Unsafe(const TSet<uint64>& InEdges, const int32 InIOIndex)
	{
		uint32 A;
		uint32 B;
		for (const uint64& E : InEdges)
		{
			if (UniqueEdges.Contains(E)) { continue; }

			PCGEx::H64(E, A, B);

			check(A != B)

			const int32 EdgeIndex = Edges.Emplace(Edges.Num(), A, B);
			UniqueEdges.Add(E, EdgeIndex);
			Nodes[A].LinkEdge(EdgeIndex);
			Nodes[B].LinkEdge(EdgeIndex);
			Edges[EdgeIndex].IOIndex = InIOIndex;
		}
	}

	void FGraph::InsertEdges(const TSet<uint64>& InEdges, const int32 InIOIndex)
	{
		FWriteScopeLock WriteLock(GraphLock);
		InsertEdges_Unsafe(InEdges, InIOIndex);
	}

	TArrayView<FNode> FGraph::AddNodes(const int32 NumNewNodes)
	{
		const int32 StartIndex = Nodes.Num();
		Nodes.SetNum(StartIndex + NumNewNodes);
		for (int i = 0; i < NumNewNodes; i++)
		{
			FNode& Node = Nodes[StartIndex + i];
			Node.Index = Node.PointIndex = StartIndex + i;
			Node.Links.Reserve(NumEdgesReserve);
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

			if (!CurrentNode.bValid || CurrentNode.IsEmpty()) { continue; }

			PCGEX_MAKE_SHARED(SubGraph, FSubGraph)
			SubGraph->WeakParentGraph = SharedThis(this);

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

				for (const FLink Lk : Node.Links)
				{
					const int32 E = Lk.Edge;

					if (VisitedEdges[E]) { continue; }
					VisitedEdges[E] = true;

					const FEdge& Edge = Edges[E];

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
			else if (!SubGraph->Edges.IsEmpty()) { SubGraphs.Add(SubGraph.ToSharedRef()); }
		}
	}

	void FGraph::GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth) const
	{
		const int32 NextDepth = SearchDepth - 1;
		const FNode& RootNode = Nodes[FromIndex];

		for (const FLink Lk : RootNode.Links)
		{
			const FEdge& Edge = Edges[Lk.Edge];
			if (!Edge.bValid) { continue; }

			int32 OtherIndex = Edge.Other(FromIndex);
			if (OutIndices.Contains(OtherIndex)) { continue; }

			OutIndices.Add(OtherIndex);
			if (NextDepth > 0) { GetConnectedNodes(OtherIndex, OutIndices, NextDepth); }
		}
	}

	void FGraphBuilder::CompileAsync(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager, const bool bWriteNodeFacade, const FGraphMetadataDetails* MetadataDetails)
	{
		AsyncManager = InAsyncManager;
		PCGEX_SHARED_THIS_DECL
		PCGEX_LAUNCH(PCGExGraphTask::FCompileGraph, ThisPtr, bWriteNodeFacade, MetadataDetails)
	}

	void FGraphBuilder::Compile(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager, const bool bWriteNodeFacade, const FGraphMetadataDetails* MetadataDetails)
	{
		check(!bCompiling)

		bCompiling = true;
		AsyncManager = InAsyncManager;
		MetadataDetailsPtr = MetadataDetails;
		bWriteVtxDataFacadeWithCompile = bWriteNodeFacade;

		TRACE_CPUPROFILER_EVENT_SCOPE(FGraphBuilder::Compile);

		NodeIndexLookup = MakeShared<PCGEx::FIndexLookup>(Graph->Nodes.Num()); // Likely larger than exported size; required for compilation.
		Graph->NodeIndexLookup = NodeIndexLookup;

		Graph->BuildSubGraphs(*OutputDetails);

		if (Graph->SubGraphs.IsEmpty())
		{
			bCompiledSuccessfully = false;
			if (OnCompilationEndCallback)
			{
				PCGEX_SHARED_THIS_DECL
				OnCompilationEndCallback(ThisPtr.ToSharedRef(), bCompiledSuccessfully);
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
					if (!Node.bValid || Node.IsEmpty()) { continue; }
					Node.PointIndex = PrunedPoints.Add(MutablePoints[Node.PointIndex]);
					ValidNodes.Add(Node.Index);
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
					if (!Node.bValid || Node.IsEmpty()) { continue; }
					Node.PointIndex = MutablePoints.Add(OriginalPoints[Node.PointIndex]);
					OutPointsMetadata->InitializeOnSet(MutablePoints.Last().MetadataEntry);
					ValidNodes.Add(Node.Index);
				}
			}

			ValidNodes.Shrink();
		}

		const TSharedPtr<PCGExData::TBuffer<int64>> VtxEndpointWriter = NodeDataFacade->GetWritable<int64>(Tag_VtxEndpoint, 0, false, PCGExData::EBufferInit::New);

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
#define PCGEX_NODE_METADATA_DECL(_NAME, _TYPE, _DEFAULT, _ACCESSOR) const TSharedPtr<PCGExData::TBuffer<_TYPE>> _NAME##Buffer = MetadataDetails->bWrite##_NAME ? NodeDataFacade->GetWritable<_TYPE>(MetadataDetails->_NAME##AttributeName, _DEFAULT, true, PCGExData::EBufferInit::New) : nullptr;
#define PCGEX_NODE_METADATA_OUTPUT(_NAME, _TYPE, _DEFAULT, _ACCESSOR) if(_NAME##Buffer){_NAME##Buffer->GetMutable(PointIndex) = NodeMeta->_ACCESSOR;}

			PCGEX_FOREACH_NODE_METADATA(PCGEX_NODE_METADATA_DECL)

			for (const int32 NodeIndex : ValidNodes)
			{
				const FGraphNodeMetadata* NodeMeta = Graph->FindNodeMetadata_Unsafe(NodeIndex);

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

			check(!SubGraph->Edges.IsEmpty())

			TSharedPtr<PCGExData::FPointIO> EdgeIO;

			if (const int32 IOIndex = SubGraph->GetFirstInIOIndex(); SubGraph->EdgesInIOIndices.Num() == 1 && SourceEdgeFacades && SourceEdgeFacades->IsValidIndex(IOIndex))
			{
				// Don't grab original point IO if we have metadata.
				EdgeIO = EdgesIO->Emplace_GetRef<UPCGExClusterEdgesData>((*SourceEdgeFacades)[IOIndex]->Source, PCGExData::EIOInit::New);
			}
			else
			{
				EdgeIO = EdgesIO->Emplace_GetRef<UPCGExClusterEdgesData>(PCGExData::EIOInit::New);
			}

			if (!EdgeIO) { return; }

			SubGraph->UID = EdgeIO->GetOut()->GetUniqueID();
			SubGraph->OnSubGraphPostProcess = OnSubGraphPostProcess;

			SubGraph->VtxDataFacade = NodeDataFacade;
			SubGraph->EdgesDataFacade = MakeShared<PCGExData::FFacade>(EdgeIO.ToSharedRef());

			MarkClusterEdges(EdgeIO, PairId);
		}

		MarkClusterVtx(NodeDataFacade->Source, PairId);

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ProcessSubGraphTask)

		ProcessSubGraphTask->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				if (This->bWriteVtxDataFacadeWithCompile)
				{
					if (This->OnCompilationEndCallback)
					{
						if (!This->bCompiledSuccessfully)
						{
							This->OnCompilationEndCallback(This.ToSharedRef(), false);
						}
						else
						{
							This->NodeDataFacade->WriteBuffers(
								This->AsyncManager,
								[AsyncThis]()
								{
									PCGEX_ASYNC_NESTED_THIS
									NestedThis->OnCompilationEndCallback(NestedThis.ToSharedRef(), true);
								});
						}
					}
					else if (This->bCompiledSuccessfully)
					{
						This->NodeDataFacade->Write(This->AsyncManager);
					}
				}
				else if (This->OnCompilationEndCallback)
				{
					This->OnCompilationEndCallback(This.ToSharedRef(), This->bCompiledSuccessfully);
				}
			};

		ProcessSubGraphTask->OnIterationCallback =
			[PCGEX_ASYNC_THIS_CAPTURE, WeakGroup = ProcessSubGraphTask](const int32 Index, const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				const TSharedPtr<FSubGraph> SubGraph = This->Graph->SubGraphs[Index];
				SubGraph->Compile(WeakGroup, This->AsyncManager, This);
			};

		ProcessSubGraphTask->StartIterations(Graph->SubGraphs.Num(), 1, false);
	}

	void FGraphBuilder::StageEdgesOutputs() const
	{
		EdgesIO->StageOutputs();
	}
}

namespace PCGExGraphTask
{
	void FWriteSubGraphCluster::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		UPCGExClusterEdgesData* ClusterEdgesData = Cast<UPCGExClusterEdgesData>(SubGraph->EdgesDataFacade->GetOut());
		const TSharedPtr<PCGExGraph::FGraph> ParentGraph = SubGraph->WeakParentGraph.Pin();
		if (!ParentGraph) { return; }
		PCGEX_MAKE_SHARED(NewCluster, PCGExCluster::FCluster, SubGraph->VtxDataFacade->Source, SubGraph->EdgesDataFacade->Source, ParentGraph->NodeIndexLookup)
		ClusterEdgesData->SetBoundCluster(NewCluster);

		SubGraph->BuildCluster(NewCluster.ToSharedRef());
	}

	void FCompileGraph::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCompileGraph::ExecuteTask);
		Builder->Compile(AsyncManager, bWriteNodeFacade, MetadataDetails);
	}

	void FCopyGraphToPoint::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		if (!GraphBuilder->bCompiledSuccessfully) { return; }

		const TSharedPtr<PCGExData::FPointIO> VtxDupe = VtxCollection->Emplace_GetRef(GraphBuilder->NodeDataFacade->GetOut(), PCGExData::EIOInit::Duplicate);
		if (!VtxDupe) { return; }

		VtxDupe->IOIndex = TaskIndex;

		PCGExTags::IDType OutId;
		PCGExGraph::SetClusterVtx(VtxDupe, OutId);

		PCGEX_MAKE_SHARED(VtxTask, PCGExGeoTasks::FTransformPointIO, TaskIndex, PointIO, VtxDupe, TransformDetails);
		Launch(VtxTask);

		for (const TSharedPtr<PCGExData::FPointIO>& Edges : GraphBuilder->EdgesIO->Pairs)
		{
			TSharedPtr<PCGExData::FPointIO> EdgeDupe = EdgeCollection->Emplace_GetRef(Edges->GetOut(), PCGExData::EIOInit::Duplicate);
			if (!EdgeDupe) { return; }

			EdgeDupe->IOIndex = TaskIndex;
			PCGExGraph::MarkClusterEdges(EdgeDupe, OutId);

			PCGEX_MAKE_SHARED(EdgeTask, PCGExGeoTasks::FTransformPointIO, TaskIndex, PointIO, EdgeDupe, TransformDetails);
			Launch(EdgeTask);
		}

		// TODO : Copy & Transform cluster as well for a big perf boost
	}
}
