// Copyright 2025 Timothé Lapetite and contributors
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

FPCGExGraphBuilderDetails::FPCGExGraphBuilderDetails(const EPCGExMinimalAxis InDefaultSolidificationAxis)
{
	BasicEdgeSolidification.SolidificationAxis = InDefaultSolidificationAxis;
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

bool PCGExGraph::BuildIndexedEdges(
	const TSharedPtr<PCGExData::FPointIO>& EdgeIO,
	const TMap<uint32, int32>& EndpointsLookup,
	TArray<FEdge>& OutEdges,
	const bool bStopOnError)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExEdge::BuildIndexedEdges-Vanilla);

	const TUniquePtr<PCGExData::TBuffer<int64>> EndpointsBuffer = MakeUnique<PCGExData::TBuffer<int64>>(EdgeIO.ToSharedRef(), Attr_PCGExEdgeIdx);
	if (!EndpointsBuffer->PrepareRead()) { return false; }

	const TArray<int64>& Endpoints = *EndpointsBuffer->GetInValues().Get();
	const int32 EdgeIOIndex = EdgeIO->IOIndex;

	bool bValid = true;
	const int32 NumEdges = EdgeIO->GetNum();

	PCGEx::InitArray(OutEdges, NumEdges);

	if (!bStopOnError)
	{
		int32 EdgeIndex = 0;

		for (int i = 0; i < NumEdges; i++)
		{
			uint32 A;
			uint32 B;
			PCGEx::H64(Endpoints[i], A, B);

			const int32* StartPointIndexPtr = EndpointsLookup.Find(A);
			const int32* EndPointIndexPtr = EndpointsLookup.Find(B);

			if ((!StartPointIndexPtr || !EndPointIndexPtr)) { continue; }

			OutEdges[EdgeIndex] = FEdge(EdgeIndex, *StartPointIndexPtr, *EndPointIndexPtr, i, EdgeIOIndex);
			EdgeIndex++;
		}

		PCGEx::InitArray(OutEdges, EdgeIndex);
	}
	else
	{
		for (int i = 0; i < NumEdges; i++)
		{
			uint32 A;
			uint32 B;
			PCGEx::H64(Endpoints[i], A, B);

			const int32* StartPointIndexPtr = EndpointsLookup.Find(A);
			const int32* EndPointIndexPtr = EndpointsLookup.Find(B);

			if ((!StartPointIndexPtr || !EndPointIndexPtr))
			{
				bValid = false;
				break;
			}

			OutEdges[i] = FEdge(i, *StartPointIndexPtr, *EndPointIndexPtr, i, EdgeIOIndex);
		}
	}

	return bValid;
}

namespace PCGExGraph
{
#define PCGEX_FOREACH_POINTEDGE_METADATA(MACRO)\
MACRO(IsIntersector, bWriteIsIntersector, IsIntersector,TEXT("bIsIntersector"))

#define PCGEX_FOREACH_EDGEEDGE_METADATA(MACRO)\
MACRO(Crossing, bWriteCrossing, Crossing,TEXT("bCrossing"))

#define PCGEX_GRAPH_META_FWD(_NAME, _ACCESSOR, _ACCESSOR2, _DEFAULT)	bWrite##_NAME = InDetails._ACCESSOR; _NAME##AttributeName = InDetails._ACCESSOR2##AttributeName; PCGEX_SOFT_VALIDATE_NAME(bWrite##_NAME, _NAME##AttributeName, Context)

	void FGraphMetadataDetails::Grab(const FPCGContext* Context, const FPCGExPointPointIntersectionDetails& InDetails)
	{
		Grab(Context, InDetails.PointUnionData);
		Grab(Context, InDetails.EdgeUnionData);
	}

	void FGraphMetadataDetails::Grab(const FPCGContext* Context, const FPCGExPointEdgeIntersectionDetails& InDetails)
	{
		PCGEX_FOREACH_POINTEDGE_METADATA(PCGEX_GRAPH_META_FWD);
	}

	void FGraphMetadataDetails::Grab(const FPCGContext* Context, const FPCGExEdgeEdgeIntersectionDetails& InDetails)
	{
		PCGEX_FOREACH_EDGEEDGE_METADATA(PCGEX_GRAPH_META_FWD);
	}

	void FGraphMetadataDetails::Grab(const FPCGContext* Context, const FPCGExPointUnionMetadataDetails& InDetails)
	{
		bWriteIsPointUnion = InDetails.bWriteIsUnion;
		IsPointUnionAttributeName = InDetails.IsUnionAttributeName;
		PCGEX_SOFT_VALIDATE_NAME(bWriteIsPointUnion, IsPointUnionAttributeName, Context)

		bWritePointUnionSize = InDetails.bWriteUnionSize;
		PointUnionSizeAttributeName = InDetails.UnionSizeAttributeName;
		PCGEX_SOFT_VALIDATE_NAME(bWritePointUnionSize, PointUnionSizeAttributeName, Context)
	}

	void FGraphMetadataDetails::Grab(const FPCGContext* Context, const FPCGExEdgeUnionMetadataDetails& InDetails)
	{
		bWriteIsEdgeUnion = InDetails.bWriteIsUnion;
		IsEdgeUnionAttributeName = InDetails.IsUnionAttributeName;
		PCGEX_SOFT_VALIDATE_NAME(bWriteIsEdgeUnion, IsEdgeUnionAttributeName, Context)

		bWriteEdgeUnionSize = InDetails.bWriteUnionSize;
		EdgeUnionSizeAttributeName = InDetails.UnionSizeAttributeName;
		PCGEX_SOFT_VALIDATE_NAME(bWriteEdgeUnionSize, EdgeUnionSizeAttributeName, Context);
	}

#undef PCGEX_FOREACH_POINTEDGE_METADATA
#undef PCGEX_FOREACH_EDGEEDGE_METADATA
#undef PCGEX_GRAPH_META_FWD

	bool FGraphNodeMetadata::IsUnion() const
	{
		return UnionSize > 1;
	}

	FGraphNodeMetadata::FGraphNodeMetadata(const int32 InNodeIndex)
		: NodeIndex(InNodeIndex)
	{
	}

	bool FGraphNodeMetadata::IsIntersector() const
	{
		return Type == EPCGExIntersectionType::PointEdge;
	}

	bool FGraphNodeMetadata::IsCrossing() const
	{
		return Type == EPCGExIntersectionType::EdgeEdge;
	}

	bool FGraphEdgeMetadata::IsUnion() const
	{
		return UnionSize > 1;
	}

	FGraphEdgeMetadata::FGraphEdgeMetadata(const int32 InEdgeIndex, const FGraphEdgeMetadata* Parent)
		: EdgeIndex(InEdgeIndex), ParentIndex(Parent ? Parent->EdgeIndex : InEdgeIndex), RootIndex(Parent ? Parent->RootIndex : InEdgeIndex)
	{
	}

	FNode::FNode(const int32 InNodeIndex, const int32 InPointIndex)
		: Index(InNodeIndex), PointIndex(InPointIndex)
	{
		Links.Empty();
	}

	bool FNode::IsAdjacentTo(const int32 OtherNodeIndex) const
	{
		for (const FLink Lk : Links) { if (Lk.Node == OtherNodeIndex) { return true; } }
		return false;
	}

	int32 FNode::GetEdgeIndex(const int32 AdjacentNodeIndex) const
	{
		for (const FLink Lk : Links) { if (Lk.Node == AdjacentNodeIndex) { return Lk.Edge; } }
		return -1;
	}

	void FSubGraph::Add(const FEdge& Edge, FGraph* InGraph)
	{
		Nodes.Add(Edge.Start);
		Nodes.Add(Edge.End);
		Edges.Add(Edge.Index);
		if (Edge.IOIndex >= 0) { EdgesInIOIndices.Add(Edge.IOIndex); }
	}

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

		if (InBuilder->OutputDetails->bOutputEdgeLength)
		{
			if (!PCGEx::IsValidName(InBuilder->OutputDetails->EdgeLengthName))
			{
				PCGE_LOG_C(Error, GraphAndLog, AsyncManager->GetContext(), FTEXT("Invalid user-defined attribute name for Edge Length."));
			}
			else
			{
				EdgeLength = EdgesDataFacade->GetWritable<double>(InBuilder->OutputDetails->EdgeLengthName, 0, true, PCGExData::EBufferInit::New);
			}
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

		const TSharedPtr<PCGExData::TBuffer<int64>> EdgeEndpointsWriter = EdgesDataFacade->GetWritable<int64>(Attr_PCGExEdgeIdx, -1, false, PCGExData::EBufferInit::New);

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
			const FPCGPoint& StartPt = Vertices[E.Start];
			const FPCGPoint& EndPt = Vertices[E.End];

			if (bHasUnionMetadata)
			{
				if (const FGraphEdgeMetadata* EdgeMeta = ParentGraph->FindRootEdgeMetadata_Unsafe(E.IOIndex))
				{
					if (TSharedPtr<PCGExData::FUnionData> UnionData = ParentGraph->EdgesUnion->Get(EdgeMeta->RootIndex);
						UnionBlender && UnionData) { UnionBlender->MergeSingle(EdgeIndex, UnionData, Distances); }

#define PCGEX_EDGE_METADATA_OUTPUT(_NAME, _TYPE, _DEFAULT, _ACCESSOR) if(_NAME##Buffer){_NAME##Buffer->GetMutable(EdgeIndex) = EdgeMeta->_ACCESSOR;}
					PCGEX_FOREACH_EDGE_METADATA(PCGEX_EDGE_METADATA_OUTPUT)
#undef PCGEX_EDGE_METADATA_OUTPUT
				}
			}

			EdgeEndpointsWriter->GetMutable(EdgeIndex) = PCGEx::H64(NodeGUID(BaseGUID, E.Start), NodeGUID(BaseGUID, E.End));

			if (Builder->OutputDetails->bWriteEdgePosition)
			{
				Builder->OutputDetails->BasicEdgeSolidification.Mutate(EdgePt, StartPt, EndPt, Builder->OutputDetails->EdgePosition);
			}

			if (EdgeLength) { EdgeLength->GetMutable(EdgeIndex) = FVector::Dist(StartPt.Transform.GetLocation(), EndPt.Transform.GetLocation()); }


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

	FGraph::FGraph(const int32 InNumNodes, const int32 InNumEdgesReserve)
		: NumEdgesReserve(InNumEdgesReserve)
	{
		PCGEX_LOG_CTR(FGraph)

		PCGEx::InitArray(Nodes, InNumNodes);

		for (int i = 0; i < InNumNodes; i++)
		{
			FNode& Node = Nodes[i];
			Node.Index = Node.PointIndex = i;
			Node.Links.Reserve(NumEdgesReserve);
		}
	}

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
		if (!InsertEdge_Unsafe(Edge.Start, Edge.End, OutEdge, InIOIndex))
		{
			OutEdge = Edge;
			return false;
		}

		return true;
	}

	bool FGraph::InsertEdge(const FEdge& Edge, FEdge& OutEdge, const int32 InIOIndex)
	{
		if (!InsertEdge(Edge.Start, Edge.End, OutEdge, InIOIndex))
		{
			OutEdge = Edge;
			return false;
		}

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

	FEdge* FGraph::FindEdge_Unsafe(const uint64 Hash)
	{
		const int32* Index = UniqueEdges.Find(Hash);
		if (!Index) { return nullptr; }
		return (Edges.GetData() + *Index);
	}

	FEdge* FGraph::FindEdge_Unsafe(const int32 A, const int32 B)
	{
		return FindEdge(PCGEx::H64U(A, B));
	}

	FEdge* FGraph::FindEdge(const uint64 Hash)
	{
		FReadScopeLock ReadScopeLock(GraphLock);
		const int32* Index = UniqueEdges.Find(Hash);
		if (!Index) { return nullptr; }
		return (Edges.GetData() + *Index);
	}

	FEdge* FGraph::FindEdge(const int32 A, const int32 B)
	{
		return FindEdge(PCGEx::H64U(A, B));
	}

	FGraphEdgeMetadata& FGraph::GetOrCreateEdgeMetadata_Unsafe(const int32 EdgeIndex, const FGraphEdgeMetadata* Parent)
	{
		if (FGraphEdgeMetadata* MetadataPtr = EdgeMetadata.Find(EdgeIndex)) { return *MetadataPtr; }
		return EdgeMetadata.Add(EdgeIndex, FGraphEdgeMetadata(EdgeIndex, Parent));
	}

	FGraphEdgeMetadata& FGraph::GetOrCreateEdgeMetadata(const int32 EdgeIndex, const FGraphEdgeMetadata* Parent)
	{
		{
			FReadScopeLock ReadScopeLock(EdgeMetadataLock);
			if (FGraphEdgeMetadata* MetadataPtr = EdgeMetadata.Find(EdgeIndex)) { return *MetadataPtr; }
		}
		{
			FWriteScopeLock WriteScopeLock(EdgeMetadataLock);
			if (FGraphEdgeMetadata* MetadataPtr = EdgeMetadata.Find(EdgeIndex)) { return *MetadataPtr; }
			return EdgeMetadata.Add(EdgeIndex, FGraphEdgeMetadata(EdgeIndex, Parent));
		}
	}

	FGraphNodeMetadata& FGraph::GetOrCreateNodeMetadata_Unsafe(const int32 NodeIndex)
	{
		if (FGraphNodeMetadata* MetadataPtr = NodeMetadata.Find(NodeIndex)) { return *MetadataPtr; }
		return NodeMetadata.Add(NodeIndex, FGraphNodeMetadata(NodeIndex));
	}

	FGraphNodeMetadata& FGraph::GetOrCreateNodeMetadata(const int32 NodeIndex)
	{
		{
			FReadScopeLock ReadScopeLock(NodeMetadataLock);
			if (FGraphNodeMetadata* MetadataPtr = NodeMetadata.Find(NodeIndex)) { return *MetadataPtr; }
		}
		{
			FWriteScopeLock WriteScopeLock(NodeMetadataLock);
			if (FGraphNodeMetadata* MetadataPtr = NodeMetadata.Find(NodeIndex)) { return *MetadataPtr; }
			return NodeMetadata.Add(NodeIndex, FGraphNodeMetadata(NodeIndex));
		}
	}

	void FGraph::AddNodeAndEdgeMetadata_Unsafe(const int32 InNodeIndex, const int32 InEdgeIndex, const FGraphEdgeMetadata* InParentMetadata, const EPCGExIntersectionType InType)
	{
		FGraphNodeMetadata& N = GetOrCreateNodeMetadata_Unsafe(InNodeIndex);
		N.Type = InType;

		FGraphEdgeMetadata& E = GetOrCreateEdgeMetadata_Unsafe(InEdgeIndex, InParentMetadata);
		E.Type = InType;
	}

	void FGraph::AddNodeAndEdgeMetadata(const int32 InNodeIndex, const int32 InEdgeIndex, const FGraphEdgeMetadata* InParentMetadata, const EPCGExIntersectionType InType)
	{
		FWriteScopeLock WriteEdgeScopeLock(EdgeMetadataLock);
		FWriteScopeLock WriteNodeScopeLock(NodeMetadataLock);
		AddNodeAndEdgeMetadata_Unsafe(InNodeIndex, InEdgeIndex, InParentMetadata, InType);
	}

	void FGraph::AddNodeMetadata_Unsafe(const int32 InNodeIndex, const FGraphEdgeMetadata* InParentMetadata, const EPCGExIntersectionType InType)
	{
		FGraphNodeMetadata& N = GetOrCreateNodeMetadata_Unsafe(InNodeIndex);
		N.Type = InType;
	}

	void FGraph::AddNodeMetadata(const int32 InNodeIndex, const FGraphEdgeMetadata* InParentMetadata, const EPCGExIntersectionType InType)
	{
		FWriteScopeLock WriteScopeLock(NodeMetadataLock);
		AddNodeMetadata_Unsafe(InNodeIndex, InParentMetadata, InType);
	}

	void FGraph::AddEdgeMetadata_Unsafe(const int32 InEdgeIndex, const FGraphEdgeMetadata* InParentMetadata, const EPCGExIntersectionType InType)
	{
		FGraphEdgeMetadata& E = GetOrCreateEdgeMetadata_Unsafe(InEdgeIndex, InParentMetadata);
		E.Type = InType;
	}

	void FGraph::AddEdgeMetadata(const int32 InEdgeIndex, const FGraphEdgeMetadata* InParentMetadata, const EPCGExIntersectionType InType)
	{
		FWriteScopeLock WriteScopeLock(EdgeMetadataLock);
		AddEdgeMetadata_Unsafe(InEdgeIndex, InParentMetadata, InType);
	}

	FGraphNodeMetadata* FGraph::FindNodeMetadata(const int32 NodeIndex)
	{
		FReadScopeLock ReadScopeLock(NodeMetadataLock);
		return FindNodeMetadata_Unsafe(NodeIndex);
	}

	FGraphEdgeMetadata* FGraph::FindEdgeMetadata(const int32 EdgeIndex)
	{
		FReadScopeLock ReadScopeLock(EdgeMetadataLock);
		return FindEdgeMetadata_Unsafe(EdgeIndex);
	}

	FGraphEdgeMetadata* FGraph::FindRootEdgeMetadata_Unsafe(const int32 EdgeIndex)
	{
		const FGraphEdgeMetadata* BaseEdge = EdgeMetadata.Find(EdgeIndex);
		return BaseEdge ? EdgeMetadata.Find(BaseEdge->RootIndex) : nullptr;
	}

	FGraphEdgeMetadata* FGraph::FindRootEdgeMetadata(const int32 EdgeIndex)
	{
		FReadScopeLock ReadScopeLock(EdgeMetadataLock);
		return FindRootEdgeMetadata_Unsafe(EdgeIndex);
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

	FGraphBuilder::FGraphBuilder(
		const TSharedRef<PCGExData::FFacade>& InNodeDataFacade,
		const FPCGExGraphBuilderDetails* InDetails,
		const int32 NumEdgeReserve)
		: OutputDetails(InDetails),
		  NodeDataFacade(InNodeDataFacade)
	{
		PCGEX_LOG_CTR(FGraphBuilder)

		PairId = NodeDataFacade->Source->Tags->Set<int32>(TagStr_PCGExCluster, NodeDataFacade->Source->GetOutIn()->GetUniqueID());

		const int32 NumNodes = NodeDataFacade->Source->GetOutInNum();

		Graph = MakeShared<FGraph>(NumNodes, NumEdgeReserve);
		Graph->bBuildClusters = InDetails->WantsClusters();
		Graph->bRefreshEdgeSeed = OutputDetails->bRefreshEdgeSeed;

		EdgesIO = MakeShared<PCGExData::FPointIOCollection>(NodeDataFacade->Source->GetContext());
		EdgesIO->OutputPin = OutputEdgesLabel;
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
		TArray<PCGEx::TOrder<FVector>> Order;

		int32 NumNodes = Nodes.Num();

		if (OutputNodeIndices) { ValidNodes = *OutputNodeIndices; }

		ValidNodes.Reserve(NumNodes);
		Order.Reserve(NumNodes);

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

					Order.Emplace(ValidNodes.Add(Node.Index), MutablePoints[Node.PointIndex].Transform.GetLocation());
				}

				NodeDataFacade->GetOut()->SetPoints(PrunedPoints);
				//TArray<FPCGPoint>& ValidPoints = NodeDataFacade->GetOut()->GetMutablePoints();
				//for (FPCGPoint& ValidPoint : ValidPoints) { OutPointsMetadata->InitializeOnSet(ValidPoint.MetadataEntry); }
			}
			else
			{
				MutablePoints.Reserve(NumNodes);

				const TArray<FPCGPoint> OriginalPoints = NodeDataFacade->GetIn()->GetPoints();

				for (FNode& Node : Nodes)
				{
					if (!Node.bValid || Node.IsEmpty()) { continue; }
					Node.PointIndex = MutablePoints.Add(OriginalPoints[Node.PointIndex]);
					//OutPointsMetadata->InitializeOnSet(MutablePoints.Last().MetadataEntry);

					Order.Emplace(ValidNodes.Add(Node.Index), MutablePoints[Node.PointIndex].Transform.GetLocation());
				}
			}

			ValidNodes.Shrink();
			Order.Shrink();

			NumNodes = ValidNodes.Num();

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FCompileGraph::SortPoints);

				Order.Sort(
					[](const PCGEx::TOrder<FVector>& A, const PCGEx::TOrder<FVector>& B)
					{
						if (A.Det.X != B.Det.X) { return A.Det.X < B.Det.X; }
						if (A.Det.Y != B.Det.Y) { return A.Det.Y < B.Det.Y; }
						return A.Det.Z < B.Det.Z;
					});

				// Reorder output points
				ReorderArray(MutablePoints, Order);
				// Reorder output indices if provided
				// Needed for delaunay etc that rely on original indices to identify sites etc
				if (OutputPointIndices && OutputPointIndices->Num() == MutablePoints.Num()) { ReorderArray(*OutputPointIndices, Order); }
			}
		}

		// Sort points & update node PointIndex

		const TSharedPtr<PCGExData::TBuffer<int64>> VtxEndpointWriter = NodeDataFacade->GetWritable<int64>(Attr_PCGExVtxIdx, 0, false, PCGExData::EBufferInit::New);

		const uint32 BaseGUID = NodeDataFacade->GetOut()->GetUniqueID();

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FCompileGraph::RemapNodes);

			for (int i = 0; i < NumNodes; i++)
			{
				FNode& Node = Nodes[ValidNodes[Order[i].Index]];
				Node.PointIndex = i;
				VtxEndpointWriter->GetMutable(Node.PointIndex) = PCGEx::H64(NodeGUID(BaseGUID, Node.PointIndex), Node.NumExportedEdges);
			}
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

			EdgeIO->IOIndex = i;

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

	bool BuildEndpointsLookup(const TSharedPtr<PCGExData::FPointIO>& InPointIO, TMap<uint32, int32>& OutIndices, TArray<int32>& OutAdjacency)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExGraph::BuildLookupTable);

		PCGEx::InitArray(OutAdjacency, InPointIO->GetNum());
		OutIndices.Empty();

		const TUniquePtr<PCGExData::TBuffer<int64>> IndexBuffer = MakeUnique<PCGExData::TBuffer<int64>>(InPointIO.ToSharedRef(), Attr_PCGExVtxIdx);
		if (!IndexBuffer->PrepareRead()) { return false; }

		const TArray<int64>& Indices = *IndexBuffer->GetInValues().Get();

		OutIndices.Reserve(Indices.Num());
		for (int i = 0; i < Indices.Num(); i++)
		{
			uint32 A;
			uint32 B;
			PCGEx::H64(Indices[i], A, B);

			OutIndices.Add(A, i);
			OutAdjacency[i] = B;
		}

		return true;
	}

	bool IsPointDataVtxReady(const UPCGMetadata* Metadata)
	{
		return Metadata->GetConstTypedAttribute<int64>(Attr_PCGExVtxIdx) ? true : false;
	}

	bool IsPointDataEdgeReady(const UPCGMetadata* Metadata)
	{
		return Metadata->GetConstTypedAttribute<int64>(Attr_PCGExEdgeIdx) ? true : false;
	}

	void CleanupVtxData(const TSharedPtr<PCGExData::FPointIO>& PointIO)
	{
		UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;
		PointIO->Tags->Remove(TagStr_PCGExCluster);
		PointIO->Tags->Remove(TagStr_PCGExVtx);
		Metadata->DeleteAttribute(Attr_PCGExVtxIdx);
		Metadata->DeleteAttribute(Attr_PCGExEdgeIdx);
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
