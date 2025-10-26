// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExGraph.h"

#include "PCGExContext.h"
#include "PCGExRandom.h"
#include "PCGExMT.h"
#include "Data/PCGExData.h"
#include "PCGExPointsProcessor.h"
#include "Details/PCGExDetailsIntersection.h"
#include "Data/Blending/PCGExUnionBlender.h"
#include "Metadata/PCGMetadata.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Data/PCGExClusterData.h"

#include "Async/ParallelFor.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExUnionData.h"
#include "Details/PCGExDetailsDistances.h"
#include "Geometry/PCGExGeo.h"

void FPCGExBasicEdgeSolidificationDetails::Mutate(PCGExData::FMutablePoint& InEdgePoint, const PCGExData::FConstPoint& InStart, const PCGExData::FConstPoint& InEnd, const double InLerp) const
{
	const FVector A = InStart.GetLocation();
	const FVector B = InEnd.GetLocation();

	InEdgePoint.SetLocation(FMath::Lerp(A, B, InLerp));
	if (SolidificationAxis == EPCGExMinimalAxis::None) { return; }

	const FVector EdgeDirection = (A - B).GetSafeNormal();

	const double EdgeLength = FVector::Dist(A, B);
	const double StartRadius = InStart.GetScaledExtents().Size();
	const double EndRadius = InEnd.GetScaledExtents().Size();

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

	const FVector PtScale = InEdgePoint.GetScale3D();
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

	InEdgePoint.SetTransform(FTransform(EdgeRot, FMath::Lerp(B, A, LerpInv), InEdgePoint.GetScale3D()));

	InEdgePoint.SetBoundsMin(BoundsMin);
	InEdgePoint.SetBoundsMax(BoundsMax);
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

	const TUniquePtr<PCGExData::TArrayBuffer<int64>> EndpointsBuffer = MakeUnique<PCGExData::TArrayBuffer<int64>>(EdgeIO.ToSharedRef(), Attr_PCGExEdgeIdx);
	if (!EndpointsBuffer->InitForRead()) { return false; }

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

		bWriteIsSubEdge = InDetails.bWriteIsSubEdge;
		IsSubEdgeAttributeName = InDetails.IsSubEdgeAttributeName;
		PCGEX_SOFT_VALIDATE_NAME(bWriteIsSubEdge, IsSubEdgeAttributeName, Context)

		bWriteEdgeUnionSize = InDetails.bWriteUnionSize;
		EdgeUnionSizeAttributeName = InDetails.UnionSizeAttributeName;
		PCGEX_SOFT_VALIDATE_NAME(bWriteEdgeUnionSize, EdgeUnionSizeAttributeName, Context);
	}

#undef PCGEX_FOREACH_POINTEDGE_METADATA
#undef PCGEX_FOREACH_EDGEEDGE_METADATA
#undef PCGEX_GRAPH_META_FWD

	FGraphNodeMetadata::FGraphNodeMetadata(const int32 InNodeIndex, const EPCGExIntersectionType InType)
		: NodeIndex(InNodeIndex), Type(InType)
	{
	}

	FGraphEdgeMetadata::FGraphEdgeMetadata(const int32 InEdgeIndex, const int32 InRootIndex, const EPCGExIntersectionType InType)
		: EdgeIndex(InEdgeIndex), RootIndex(InRootIndex < 0 ? InEdgeIndex : InRootIndex), Type(InType)
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

	void FSubGraph::Shrink()
	{
		Nodes.Shrink();
		Edges.Shrink();
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

		const int32 NumEdges = Edges.Num();
		TArray<int32> EdgeDump = Edges.Array();

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FWriteSubGraphEdges::EdgeSorting);


			struct FEdgeSortKey
			{
				int32 Index;
				uint64 PackedKey;

				FEdgeSortKey(int32 InIndex, int32 A, int32 B)
					: Index(InIndex)
				{
					const int32 Min = FMath::Min(A, B);
					const int32 Max = FMath::Max(A, B);
					PackedKey = (static_cast<uint64>(Min) << 32) | static_cast<uint64>(Max);
				}

				bool operator<(const FEdgeSortKey& Other) const
				{
					return PackedKey < Other.PackedKey;
				}
			};

			TArray<FEdgeSortKey> EdgeSortKeys;
			EdgeSortKeys.SetNumUninitialized(NumEdges);

			if (NumEdges < 1024)
			{
				for (int i = 0; i < NumEdges; i++)
				{
					const int32 Index = EdgeDump[i];
					const FEdge& E = ParentGraph->Edges[Index];
					const int32 A = ParentGraph->Nodes[E.Start].PointIndex;
					const int32 B = ParentGraph->Nodes[E.End].PointIndex;
					EdgeSortKeys[i] = FEdgeSortKey(Index, A, B);
				}
			}
			else
			{
				ParallelFor(
					NumEdges, [&](int32 i)
					{
						const int32 Index = EdgeDump[i];
						const FEdge& E = ParentGraph->Edges[Index];
						const int32 A = ParentGraph->Nodes[E.Start].PointIndex;
						const int32 B = ParentGraph->Nodes[E.End].PointIndex;
						EdgeSortKeys[i] = FEdgeSortKey(Index, A, B);
					});
			}

			EdgeSortKeys.Sort();
			for (int32 i = 0; i < NumEdges; ++i) { EdgeDump[i] = EdgeSortKeys[i].Index; }
		}

		FlattenedEdges.Reserve(NumEdges);

		const UPCGBasePointData* InEdgeData = EdgesDataFacade->GetIn();
		UPCGBasePointData* OutEdgeData = EdgesDataFacade->GetOut();
		(void)PCGEx::SetNumPointsAllocated(OutEdgeData, NumEdges, InEdgeData ? InEdgeData->GetAllocatedProperties() : EPCGPointNativeProperties::MetadataEntry);

		const TPCGValueRange<int64> OutMetadataEntries = OutEdgeData->GetMetadataEntryValueRange(false);

		UPCGMetadata* Metadata = OutEdgeData->MutableMetadata();

		const TArray<FNode>& ParentGraphNodes = ParentGraph->Nodes;
		const TArray<FEdge>& ParentGraphEdges = ParentGraph->Edges;

		if (InEdgeData)
		{
			// We'll cherry pick existing edges
			TRACE_CPUPROFILER_EVENT_SCOPE(FWriteSubGraphEdges::CherryPickInheritedEdges);

			TArray<int32> ReadEdgeIndices;
			TArray<int32> WriteEdgeIndices;

			ReadEdgeIndices.Reserve(NumEdges);
			WriteEdgeIndices.Reserve(NumEdges);

			const TConstPCGValueRange<int64> InMetadataEntries = InEdgeData->GetConstMetadataEntryValueRange();

			for (int i = 0; i < NumEdges; i++)
			{
				const FEdge& OE = ParentGraphEdges[EdgeDump[i]];

				// Hijack edge IOIndex to store original edge index in the flattened
				FlattenedEdges.Emplace(i, ParentGraphNodes[OE.Start].PointIndex, ParentGraph->Nodes[OE.End].PointIndex, i, OE.Index);

				const int32 OriginalPointIndex = OE.PointIndex;
				int64& EdgeMetadataEntry = OutMetadataEntries[i];

				if (InMetadataEntries.IsValidIndex(OriginalPointIndex))
				{
					// Grab existing metadata entry & cache read/write indices
					EdgeMetadataEntry = InMetadataEntries[OriginalPointIndex];
					ReadEdgeIndices.Add(OriginalPointIndex);
					WriteEdgeIndices.Add(i);
				}

				Metadata->InitializeOnSet(EdgeMetadataEntry);
			}

			EPCGPointNativeProperties Allocate = EPCGPointNativeProperties::All;
			EnumRemoveFlags(Allocate, EPCGPointNativeProperties::MetadataEntry);
			EdgesDataFacade->Source->InheritProperties(ReadEdgeIndices, WriteEdgeIndices, Allocate);
		}
		else
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FWriteSubGraphEdges::CreatePoints);

			for (int i = 0; i < NumEdges; i++)
			{
				const FEdge& E = ParentGraphEdges[EdgeDump[i]];
				FlattenedEdges.Emplace(i, ParentGraphNodes[E.Start].PointIndex, ParentGraphNodes[E.End].PointIndex, i, E.Index);
				Metadata->InitializeOnSet(OutMetadataEntries[i]);
			}
		}

		MetadataDetails = InBuilder->GetMetadataDetails();
		const bool bHasUnionMetadata = (MetadataDetails && InBuilder && !ParentGraph->EdgeMetadata.IsEmpty());

#define PCGEX_FOREACH_EDGE_METADATA(MACRO)\
MACRO(IsEdgeUnion, bool, false, IsUnion()) \
MACRO(IsSubEdge, bool, false, bIsSubEdge) \
MACRO(EdgeUnionSize, int32, 0, UnionSize)

#define PCGEX_EDGE_METADATA_DECL(_NAME, _TYPE, _DEFAULT, _ACCESSOR) if(bHasUnionMetadata && MetadataDetails->bWrite##_NAME){ _NAME##Buffer = EdgesDataFacade->GetWritable<_TYPE>(MetadataDetails->_NAME##AttributeName, _DEFAULT, true, PCGExData::EBufferInit::New); }
		PCGEX_FOREACH_EDGE_METADATA(PCGEX_EDGE_METADATA_DECL)
#undef PCGEX_EDGE_METADATA_DECL

		Distances = PCGExDetails::MakeNoneDistances();

		if (InBuilder->SourceEdgeFacades && ParentGraph->EdgesUnion)
		{
			UnionBlender = MakeShared<PCGExDataBlending::FUnionBlender>(MetadataDetails->EdgesBlendingDetailsPtr, MetadataDetails->EdgesCarryOverDetails, Distances);
			UnionBlender->AddSources(*InBuilder->SourceEdgeFacades, &ProtectedClusterAttributes);
			if (!UnionBlender->Init(AsyncManager->GetContext(), EdgesDataFacade, ParentGraph->EdgesUnion))
			{
				// TODO : Log error
				return;
			}
		}

		if (InBuilder->OutputDetails->bOutputEdgeLength)
		{
			if (!PCGEx::IsWritableAttributeName(InBuilder->OutputDetails->EdgeLengthName))
			{
				PCGE_LOG_C(Error, GraphAndLog, AsyncManager->GetContext(), FTEXT("Invalid user-defined attribute name for Edge Length."));
			}
			else
			{
				EdgeLength = EdgesDataFacade->GetWritable<double>(InBuilder->OutputDetails->EdgeLengthName, 0, true, PCGExData::EBufferInit::New);
			}
		}

		EPCGPointNativeProperties AllocateProperties = EPCGPointNativeProperties::None;

		if (InBuilder->OutputDetails->bWriteEdgePosition)
		{
			AllocateProperties |= EPCGPointNativeProperties::Transform;
		}

		if (InBuilder->OutputDetails->BasicEdgeSolidification.SolidificationAxis != EPCGExMinimalAxis::None)
		{
			AllocateProperties |= EPCGPointNativeProperties::Transform;
			AllocateProperties |= EPCGPointNativeProperties::BoundsMin;
			AllocateProperties |= EPCGPointNativeProperties::BoundsMax;
		}

		if (ParentGraph->bRefreshEdgeSeed || InBuilder->OutputDetails->bRefreshEdgeSeed)
		{
			AllocateProperties |= EPCGPointNativeProperties::Seed;
		}

		EdgesDataFacade->GetOut()->AllocateProperties(AllocateProperties);

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


		UPCGBasePointData* OutVtxData = VtxDataFacade->GetOut();
		UPCGBasePointData* OutEdgeData = EdgesDataFacade->GetOut();

		TConstPCGValueRange<FTransform> VtxTransforms = OutVtxData->GetConstTransformValueRange();

		TPCGValueRange<int32> EdgeSeeds = OutEdgeData->GetSeedValueRange(false);

		const bool bHasUnionMetadata = (MetadataDetails && !ParentGraph->EdgeMetadata.IsEmpty());
		const FVector SeedOffset = FVector(EdgesDataFacade->Source->IOIndex);

		TArray<PCGExData::FWeightedPoint> WeightedPoints;
		TArray<PCGEx::FOpStats> Trackers;

		if (UnionBlender) { UnionBlender->InitTrackers(Trackers); }

		PCGEX_SCOPE_LOOP(i)
		{
			const FEdge& E = FlattenedEdges[i];
			const int32 EdgeIndex = E.Index; // TODO : This is now i, anyway?

			const int32 Start = E.Start;
			const int32 End = E.End;

			PCGExData::FMutablePoint EdgePt = EdgesDataFacade->GetOutPoint(EdgeIndex);

			if (bHasUnionMetadata)
			{
				const FGraphEdgeMetadata* EdgeMeta = ParentGraph->FindEdgeMetadata_Unsafe(E.IOIndex);
				if (const FGraphEdgeMetadata* RootEdgeMeta = EdgeMeta ? ParentGraph->FindEdgeMetadata_Unsafe(EdgeMeta->RootIndex) : nullptr)
				{
					if (TSharedPtr<PCGExData::IUnionData> UnionData = ParentGraph->EdgesUnion->Get(RootEdgeMeta->RootIndex);
						UnionBlender && UnionData) { UnionBlender->MergeSingle(EdgeIndex, UnionData, WeightedPoints, Trackers); }

					// TODO : Add Sub-edge edge (is the result of a subdivision + merge)

					if (IsEdgeUnionBuffer) { IsEdgeUnionBuffer->SetValue(EdgeIndex, RootEdgeMeta->IsUnion() || EdgeMeta->IsUnion()); }
					if (IsSubEdgeBuffer) { IsSubEdgeBuffer->SetValue(EdgeIndex, RootEdgeMeta->bIsSubEdge || EdgeMeta->bIsSubEdge); }
					if (EdgeUnionSizeBuffer)
					{
						EdgeUnionSizeBuffer->SetValue(
							EdgeIndex, EdgeMeta != RootEdgeMeta ?
								           RootEdgeMeta->UnionSize + EdgeMeta->UnionSize :
								           RootEdgeMeta->UnionSize);
					}
				}
			}

			EdgeEndpointsWriter->SetValue(EdgeIndex, PCGEx::H64(Start, End));

			if (Builder->OutputDetails->bWriteEdgePosition)
			{
				Builder->OutputDetails->BasicEdgeSolidification.Mutate(
					EdgePt, VtxDataFacade->GetOutPoint(Start), VtxDataFacade->GetOutPoint(End),
					Builder->OutputDetails->EdgePosition);
			}

			if (EdgeLength) { EdgeLength->SetValue(EdgeIndex, FVector::Dist(VtxTransforms[Start].GetLocation(), VtxTransforms[End].GetLocation())); }

			if (EdgeSeeds[EdgeIndex] == 0 || ParentGraph->bRefreshEdgeSeed) { EdgeSeeds[EdgeIndex] = PCGExRandom::ComputeSpatialSeed(EdgePt.GetLocation(), SeedOffset); }
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
		EdgesDataFacade->WriteFastest(AsyncManager);
	}

#undef PCGEX_FOREACH_EDGE_METADATA

	FGraph::FGraph(const int32 InNumNodes)
	{
		int32 StartNodeIndex = 0;
		AddNodes(InNumNodes, StartNodeIndex);
	}

	void FGraph::ReserveForEdges(const int32 UpcomingAdditionCount, bool bReserveMeta)
	{
		UniqueEdges.Reserve(UniqueEdges.Num() + UpcomingAdditionCount);
		Edges.Reserve(Edges.Num() + UpcomingAdditionCount);

		if (bReserveMeta)
		{
			EdgeMetadata.Reserve(EdgeMetadata.Num() + UpcomingAdditionCount);
			NodeMetadata.Reserve(NodeMetadata.Num() + UpcomingAdditionCount);
		}
	}

	bool FGraph::InsertEdge_Unsafe(const int32 A, const int32 B, FEdge& OutEdge, const int32 IOIndex)
	{
		check(A != B)

		const uint64 Hash = PCGEx::H64U(A, B);
		if (const int32* EdgeIndex = UniqueEdges.Find(Hash))
		{
			OutEdge.Index = *EdgeIndex;
			return false;
		}

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
		TRACE_CPUPROFILER_EVENT_SCOPE(FGraph::InsertEdges)

		FWriteScopeLock WriteLock(GraphLock);
		uint32 A;
		uint32 B;

		UniqueEdges.Reserve(UniqueEdges.Num() + UniqueEdges.Num());
		Edges.Reserve(Edges.Num() + InEdges.Num());

		for (const uint64 E : InEdges)
		{
			if (UniqueEdges.Contains(E)) { continue; }

			PCGEx::H64(E, A, B);

			check(A != B)

			const int32 EdgeIndex = Edges.Emplace(Edges.Num(), A, B, -1, InIOIndex);

			UniqueEdges.Add(E, EdgeIndex);
			Nodes[A].LinkEdge(EdgeIndex);
			Nodes[B].LinkEdge(EdgeIndex);
		}

		UniqueEdges.Shrink();
	}

	int32 FGraph::InsertEdges(const TArray<FEdge>& InEdges)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FGraph::InsertEdges)

		FWriteScopeLock WriteLock(GraphLock);
		const int32 StartIndex = Edges.Num();

		UniqueEdges.Reserve(UniqueEdges.Num() + InEdges.Num());
		Edges.Reserve(Edges.Num() + InEdges.Num());

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

	FGraphEdgeMetadata& FGraph::GetOrCreateEdgeMetadata(const int32 EdgeIndex, const int32 RootIndex)
	{
		{
			FReadScopeLock ReadScopeLock(MetadataLock);
			if (FGraphEdgeMetadata* MetadataPtr = EdgeMetadata.Find(EdgeIndex)) { return *MetadataPtr; }
		}
		{
			FWriteScopeLock WriteScopeLock(MetadataLock);
			return EdgeMetadata.FindOrAdd(EdgeIndex, FGraphEdgeMetadata(EdgeIndex, RootIndex));
		}
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

	TArrayView<FNode> FGraph::AddNodes(const int32 NumNewNodes, int32& OutStartIndex)
	{
		FWriteScopeLock WriteLock(GraphLock);
		OutStartIndex = Nodes.Num();
		const int32 TotalNum = OutStartIndex + NumNewNodes;
		Nodes.Reserve(TotalNum);
		for (int i = OutStartIndex; i < TotalNum; i++) { Nodes.Emplace(i, i); }

		return MakeArrayView(Nodes.GetData() + OutStartIndex, NumNewNodes);
	}

	void FGraph::BuildSubGraphs(const FPCGExGraphBuilderDetails& Limits)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FGraph::BuildSubGraphs);

		TBitArray<> VisitedNodes;
		TBitArray<> VisitedEdges;
		VisitedNodes.Init(false, Nodes.Num());
		VisitedEdges.Init(false, Edges.Num());

		int32 VisitedNodesNum = 0;
		int32 VisitedEdgesNum = 0;

		TSharedPtr<FSubGraph> SubGraph = nullptr;

		for (int i = 0; i < Nodes.Num(); i++)
		{
			FNode& CurrentNode = Nodes[i];

			if (VisitedNodes[i]) { continue; }

			VisitedNodes[i] = true;
			VisitedNodesNum++;

			if (!CurrentNode.bValid) { continue; }
			if (CurrentNode.IsEmpty())
			{
				CurrentNode.bValid = false;
				continue;
			}

			if (!SubGraph)
			{
				SubGraph = MakeShared<FSubGraph>();
				SubGraph->WeakParentGraph = SharedThis(this);
				SubGraph->Nodes.Reserve(Nodes.Num() - VisitedNodesNum);
				SubGraph->Edges.Reserve(Edges.Num() - VisitedEdgesNum);
			}

			TArray<int32> Stack;
			Stack.Reserve(Nodes.Num() - VisitedNodesNum);
			Stack.Add(i);

			while (Stack.Num() > 0)
			{
				const int32 NextIndex = Stack.Pop(EAllowShrinking::No);
				FNode& Node = Nodes[NextIndex];
				Node.NumExportedEdges = 0;

				for (const FLink Lk : Node.Links)
				{
					const int32 E = Lk.Edge;

					if (VisitedEdges[E]) { continue; }
					VisitedEdges[E] = true;
					VisitedEdgesNum++;

					const FEdge& Edge = Edges[E];

					if (!Edge.bValid) { continue; }

					const int32 OtherIndex = Edge.Other(NextIndex);
					if (!Nodes[OtherIndex].bValid) { continue; }

					Node.NumExportedEdges++;
					SubGraph->Add(Edge, this);

					if (!VisitedNodes[OtherIndex]) // Only enqueue if not already visited
					{
						VisitedNodes[OtherIndex] = true;
						VisitedNodesNum++;

						Stack.Add(OtherIndex);
					}
				}
			}

			if (!Limits.IsValid(SubGraph))
			{
				// Invalidate traversed points and edges
				for (const int32 j : SubGraph->Nodes) { Nodes[j].bValid = false; }
				for (const int32 j : SubGraph->Edges) { Edges[j].bValid = false; }

				SubGraph->Nodes.Reset();
				SubGraph->Edges.Reset();
				SubGraph->EdgesInIOIndices.Reset();
			}
			else if (!SubGraph->Edges.IsEmpty())
			{
				SubGraph->Shrink();
				SubGraphs.Add(SubGraph.ToSharedRef());
				SubGraph = nullptr;
			}
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
		const FPCGExGraphBuilderDetails* InDetails)
		: OutputDetails(InDetails),
		  NodeDataFacade(InNodeDataFacade)
	{
		PCGEX_SHARED_CONTEXT_VOID(NodeDataFacade->Source->GetContextHandle())

		const UPCGBasePointData* NodePointData = NodeDataFacade->Source->GetOutIn();
		PairId = NodeDataFacade->Source->Tags->Set<int32>(TagStr_PCGExCluster, NodePointData->GetUniqueID());


		// We initialize from the number of output point if it's greater than 0 at init time
		// Otherwise, init with input points

		const int32 NumOutPoints = NodeDataFacade->Source->GetOut() ? NodeDataFacade->Source->GetNum(PCGExData::EIOSide::Out) : 0;
		int32 InitialNumNodes = 0;

		if (NumOutPoints != 0)
		{
			NodePointsTransforms = NodeDataFacade->Source->GetOut()->GetConstTransformValueRange();
			InitialNumNodes = NumOutPoints;
		}
		else
		{
			NodePointsTransforms = NodeDataFacade->Source->GetIn()->GetConstTransformValueRange();
			InitialNumNodes = NodeDataFacade->Source->GetNum(PCGExData::EIOSide::In);
		}

		check(InitialNumNodes > 0)

		Graph = MakeShared<FGraph>(InitialNumNodes);

		Graph->bBuildClusters = InDetails->WantsClusters();
		Graph->bRefreshEdgeSeed = OutputDetails->bRefreshEdgeSeed;

		EdgesIO = MakeShared<PCGExData::FPointIOCollection>(SharedContext.Get());
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

		// NOTE : We now output nodes to have readable, final positions when we compile the graph, which kindda sucks
		// It means we need to fully allocate graph data even when ultimately we might prune out a lot of it

		bCompiling = true;
		AsyncManager = InAsyncManager;
		MetadataDetailsPtr = MetadataDetails;
		bWriteVtxDataFacadeWithCompile = bWriteNodeFacade;

		TRACE_CPUPROFILER_EVENT_SCOPE(FGraphBuilder::Compile);

		NodeIndexLookup = MakeShared<PCGEx::FIndexLookup>(Graph->Nodes.Num()); // Likely larger than exported size; required for compilation.
		Graph->NodeIndexLookup = NodeIndexLookup;

		// Building subgraphs isolate connected edge clusters
		// and invalidate roaming (isolated) nodes
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

		NodeDataFacade->Source->ClearCachedKeys(); //Ensure fresh keys later on

		TArray<FNode>& Nodes = Graph->Nodes;

		TArray<int32> InternalValidNodes;
		TArray<int32>& ValidNodes = InternalValidNodes;

		int32 NumNodes = Nodes.Num();

		if (OutputNodeIndices) { ValidNodes = *OutputNodeIndices.Get(); }

		ValidNodes.Reserve(NumNodes);

		bool bHasInvalidNodes = false;

		// Filter all valid nodes
		for (FNode& Node : Nodes)
		{
			if (!Node.bValid)
			{
				bHasInvalidNodes = true;
				continue;
			}
			ValidNodes.Add(Node.Index);
		}

		const int32 NumValidNodes = ValidNodes.Num();

		TArray<int32> ReadIndices;

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FCompileGraph::PrunePoints);

			const UPCGBasePointData* InNodeData = NodeDataFacade->GetIn();
			UPCGBasePointData* OutNodeData = NodeDataFacade->GetOut();

			if (InNodeData && bInheritNodeData)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FCompileGraph::PrunePoints::Inherit);

				ReadIndices.SetNumUninitialized(NumValidNodes);

				// In order to inherit from node data
				// both input & output must be valid
				check(!InNodeData->IsEmpty())
				check(InNodeData->GetNumPoints() >= NumValidNodes)

				const bool bOutputIsSameAsInput =
					!bHasInvalidNodes &&
					NumValidNodes == InNodeData->GetNumPoints() &&
					NumValidNodes == OutNodeData->GetNumPoints();

				if (!bOutputIsSameAsInput)
				{
					// Ensure we have the required number of nodes in the output
					PCGEx::EnsureMinNumPoints(OutNodeData, NumValidNodes);

					// Build & remap new point count to node topology
					for (int i = 0; i < NumValidNodes; i++)
					{
						FNode& Node = Nodes[ValidNodes[i]];
						ReadIndices[i] = Node.PointIndex; // { NewIndex : InheritedIndex }
						Node.PointIndex = i;              // Update node point index
					}

					// Truncate output if need be
					OutNodeData->SetNumPoints(NumValidNodes);
					// Copy input to outputs to carry over the right values on the outgoing points
					NodeDataFacade->Source->InheritProperties(ReadIndices);
				}
			}
			else
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FCompileGraph::PrunePoints::New);

				// We don't have to inherit points, this sounds great
				// However it makes things harder for us because we need to enforce a deterministic layout for other cluster nodes
				// We make the assumption that if we don't inherit points, we've introduced new one nodes & edges from different threads
				// The cheap way to make things deterministic is to sort nodes by spatial position

				// Rough check to make sure we won't have a PointIndex that's outside the desired range
				check(NodePointsTransforms.Num() >= Nodes.Num())

				// We must have an output size that's at least equal to the number of nodes we have as well, to do the re-order
				check(OutNodeData->GetNumPoints() >= Nodes.Num())

				// Init array of indice as a valid order range first, will be truncated later.
				// We save a bit of memory by re-using it
				PCGEx::ArrayOfIndices(ReadIndices, OutNodeData->GetNumPoints());

				{
					TRACE_CPUPROFILER_EVENT_SCOPE(FCompileGraph::Sort);

					const int32 N = ValidNodes.Num();
					TArray<double> Xs, Ys, Zs;
					Xs.SetNumUninitialized(N);
					Ys.SetNumUninitialized(N);
					Zs.SetNumUninitialized(N);

					for (int32 i = 0; i < N; ++i)
					{
						const FNode& Node = Nodes[ValidNodes[i]];
						const FVector P = NodePointsTransforms[Node.PointIndex].GetLocation();
						Xs[i] = P.X;
						Ys[i] = P.Y;
						Zs[i] = P.Z;
					}

					// Sort valid nodes based on outgoing transforms
					ValidNodes.Sort(
						[&](const int32 A, const int32 B)
						{
							const float AX = Xs[A];
							const float BX = Xs[B];
							if (AX != BX)
							{
								return AX < BX;
							}

							const float AY = Ys[A];
							const float BY = Ys[B];
							if (AY != BY)
							{
								return AY < BY;
							}

							return Zs[A] < Zs[B];
						});
				}

				for (int i = 0; i < NumValidNodes; i++)
				{
					FNode& Node = Nodes[ValidNodes[i]];
					ReadIndices[i] = Node.PointIndex;
					Node.PointIndex = i;
				}

				// There is no points to inherit from; meaning we need to reorder the existing data
				// because it's likely to be fragmented
				PCGEx::ReorderPointArrayData(OutNodeData, ReadIndices);

				// Truncate output to the number of nodes
				OutNodeData->SetNumPoints(NumValidNodes);
			}
		}

		////////////
		//  At this point, OutPointData must be up-to-date
		//  Transforms & Metadata entry must be final and match the Nodes.PointIndex
		//	Subgraph compilation rely on it.
		///////////

		if (OutputPointIndices && OutputPointIndices->Num() == NumValidNodes)
		{
			// Reorder output indices if provided
			// Needed for delaunay etc that rely on original indices to identify sites etc
			TArray<int32>& OutputPointIndicesRef = *OutputPointIndices.Get();
			for (int32 i = 0; i < NumValidNodes; i++) { OutputPointIndicesRef[i] = ReadIndices[i]; }
		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FCompileGraph::VtxEndpoints);

			const TSharedPtr<PCGExData::TBuffer<int64>> VtxEndpointWriter = NodeDataFacade->GetWritable<int64>(Attr_PCGExVtxIdx, 0, false, PCGExData::EBufferInit::New);
			const TSharedPtr<PCGExData::TArrayBuffer<int64>> ElementsVtxEndpointWriter = StaticCastSharedPtr<PCGExData::TArrayBuffer<int64>>(VtxEndpointWriter);

			TArray<int64>& VtxEndpoints = *ElementsVtxEndpointWriter->GetOutValues().Get();
			for (const int32 ValidNodeIndex : ValidNodes)
			{
				const FNode& Node = Nodes[ValidNodeIndex];
				VtxEndpoints[Node.PointIndex] = PCGEx::H64(Node.PointIndex, Node.NumExportedEdges);
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
#define PCGEX_NODE_METADATA_OUTPUT(_NAME, _TYPE, _DEFAULT, _ACCESSOR) if(_NAME##Buffer){_NAME##Buffer->SetValue(PointIndex, NodeMeta->_ACCESSOR);}

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
						This->NodeDataFacade->WriteFastest(This->AsyncManager);
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

	void FGraphBuilder::MoveEdgesOutputs(const TSharedPtr<PCGExData::FPointIOCollection>& To, const int32 IndexOffset) const
	{
		for (const TSharedPtr<PCGExData::FPointIO>& IO : EdgesIO->Pairs)
		{
			const int32 DesiredIndex = IO->IOIndex + IndexOffset;
			To->Add(IO);
			IO->IOIndex = DesiredIndex;
		}

		EdgesIO->Pairs.Empty();
	}

	bool BuildEndpointsLookup(const TSharedPtr<PCGExData::FPointIO>& InPointIO, TMap<uint32, int32>& OutIndices, TArray<int32>& OutAdjacency)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExGraph::BuildLookupTable);

		PCGEx::InitArray(OutAdjacency, InPointIO->GetNum());
		OutIndices.Empty();

		const TUniquePtr<PCGExData::TArrayBuffer<int64>> IndexBuffer = MakeUnique<PCGExData::TArrayBuffer<int64>>(InPointIO.ToSharedRef(), Attr_PCGExVtxIdx);
		if (!IndexBuffer->InitForRead()) { return false; }

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
		return PCGEx::TryGetConstAttribute<int64>(Metadata, Attr_PCGExVtxIdx) ? true : false;
	}

	bool IsPointDataEdgeReady(const UPCGMetadata* Metadata)
	{
		return PCGEx::TryGetConstAttribute<int64>(Metadata, Attr_PCGExEdgeIdx) ? true : false;
	}

	void CleanupVtxData(const TSharedPtr<PCGExData::FPointIO>& PointIO)
	{
		if (!PointIO->GetOut()) { return; }
		UPCGMetadata* Metadata = PointIO->GetOut()->MutableMetadata();
		PointIO->Tags->Remove(TagStr_PCGExCluster);
		PointIO->Tags->Remove(TagStr_PCGExVtx);
		Metadata->DeleteAttribute(Attr_PCGExVtxIdx);
		Metadata->DeleteAttribute(Attr_PCGExEdgeIdx);
	}

	void CleanupEdgeData(const TSharedPtr<PCGExData::FPointIO>& PointIO)
	{
		if (!PointIO->GetOut()) { return; }
		UPCGMetadata* Metadata = PointIO->GetOut()->MutableMetadata();
		PointIO->Tags->Remove(TagStr_PCGExCluster);
		PointIO->Tags->Remove(TagStr_PCGExEdges);
		Metadata->DeleteAttribute(Attr_PCGExVtxIdx);
		Metadata->DeleteAttribute(Attr_PCGExEdgeIdx);
	}

	void CleanupClusterData(const TSharedPtr<PCGExData::FPointIO>& PointIO)
	{
		CleanupVtxData(PointIO);
		CleanupEdgeData(PointIO);
		CleanupClusterTags(PointIO);
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
		if (!GraphBuilder || !GraphBuilder->bCompiledSuccessfully) { return; }

		const TSharedPtr<PCGExData::FPointIO> VtxDupe = VtxCollection->Emplace_GetRef(GraphBuilder->NodeDataFacade->GetOut(), PCGExData::EIOInit::Duplicate);
		if (!VtxDupe) { return; }

		VtxDupe->IOIndex = TaskIndex;

		PCGExCommon::DataIDType OutId;
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
