// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExWriteEdgeExtras.h"

#include "Data/Blending/PCGExMetadataBlender.h"
#include "Graph/Edges/Promoting/PCGExEdgePromoteToPoint.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"
#define PCGEX_NAMESPACE WriteEdgeExtras

UPCGExWriteEdgeExtrasSettings::UPCGExWriteEdgeExtrasSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EInit UPCGExWriteEdgeExtrasSettings::GetMainOutputInitMode() const { return bWriteVtxNormal ? PCGExData::EInit::DuplicateInput : PCGExData::EInit::Forward; }

PCGExData::EInit UPCGExWriteEdgeExtrasSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(WriteEdgeExtras)

FPCGExWriteEdgeExtrasContext::~FPCGExWriteEdgeExtrasContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_DELETE)

	PCGEX_OUTPUT_DELETE(VtxNormal, FVector)
	PCGEX_DELETE(VtxDirCompGetter)
	PCGEX_DELETE(EdgeDirCompGetter)

	PCGEX_DELETE(MetadataBlender)

	ProjectionSettings.Cleanup();
}

bool FPCGExWriteEdgeExtrasElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteEdgeExtras)


	PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_VALIDATE_NAME)
	PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_FWD)

	PCGEX_OUTPUT_VALIDATE_NAME(VtxNormal, FVector)
	PCGEX_OUTPUT_FWD(VtxNormal, FVector)

	PCGEX_FWD(DirectionChoice)
	PCGEX_FWD(DirectionMethod)

	PCGEX_FWD(bWriteEdgePosition)
	PCGEX_FWD(EdgePositionLerp)

	PCGEX_FWD(bEndpointsBlending)
	PCGEX_FWD(EndpointsBlending)

	if (Context->bEndpointsBlending)
	{
		Context->MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(&Settings->BlendingSettings));
	}

	PCGEX_FWD(ProjectionSettings)

	return true;
}

bool FPCGExWriteEdgeExtrasElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteEdgeExtrasElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(WriteEdgeExtras)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->VtxDirCompGetter)

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no bound edges."));
				return false;
			}

			Context->ProjectionSettings.Init(Context->CurrentIO);

			if (Settings->DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsAttribute)
			{
				Context->VtxDirCompGetter = new PCGEx::FLocalSingleFieldGetter();
				Context->VtxDirCompGetter->Capture(Settings->VtxSourceAttribute);
				Context->VtxDirCompGetter->Grab(*Context->CurrentIO);
			}

			PCGExData::FPointIO& PointIO = *Context->CurrentIO;
			PCGEX_OUTPUT_ACCESSOR_INIT(VtxNormal, FVector)

			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		PCGEX_DELETE(Context->EdgeDirCompGetter)

		if (!Context->AdvanceEdges(true))
		{
			PCGEX_OUTPUT_WRITE(VtxNormal, FVector)
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		if (!Context->CurrentCluster)
		{
			PCGEX_INVALID_CLUSTER_LOG
			return false;
		}

		if (Settings->DirectionMethod == EPCGExEdgeDirectionMethod::EdgeDotAttribute)
		{
			Context->EdgeDirCompGetter = new PCGEx::FLocalVectorGetter();
			Context->EdgeDirCompGetter->Capture(Settings->EdgeSourceAttribute);
			Context->EdgeDirCompGetter->Grab(*Context->CurrentEdges);
		}

		PCGExData::FPointIO& PointIO = *Context->CurrentEdges;
		PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_ACCESSOR_INIT)

		if (Context->MetadataBlender) { Context->MetadataBlender->PrepareForData(PointIO, *Context->CurrentIO); }
		//Context->GetAsyncManager()->Start<FPCGExWriteExtrasTask>(-1, &PointIO, &Context->ProjectionSettings);
		//Context->SetAsyncState(PCGExGraph::State_ProcessingEdges);

		///

		if (Context->VtxNormalWriter)
		{
			PCGExCluster::FClusterProjection* ProjectedCluster = new PCGExCluster::FClusterProjection(Context->CurrentCluster, &Context->ProjectionSettings);
			ProjectedCluster->Build();

			for (PCGExCluster::FNodeProjection& Vtx : ProjectedCluster->Nodes)
			{
				Vtx.ComputeNormal(Context->CurrentCluster);
				Context->VtxNormalWriter->Values[Vtx.Node->NodeIndex] = Vtx.Normal;
			}

			PCGEX_DELETE(ProjectedCluster)
		}

		Context->bAscendingDesired = Context->DirectionChoice == EPCGExEdgeDirectionChoice::SmallestToGreatest;
		Context->StartWeight = FMath::Clamp(Context->EndpointsBlending, 0, 1);
		Context->EndWeight = 1 - Context->StartWeight;


		///


		Context->SetState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		auto ProcessEdge = [&](const int32 EdgeIndex)
		{
			const PCGExGraph::FIndexedEdge& Edge = Context->CurrentCluster->Edges[EdgeIndex];
			const FPCGPoint& StartPoint = Context->CurrentIO->GetInPoint(Edge.Start);
			const FPCGPoint& EndPoint = Context->CurrentIO->GetInPoint(Edge.End);

			const FVector DirFrom = StartPoint.Transform.GetLocation();
			const FVector DirTo = EndPoint.Transform.GetLocation();
			bool bAscending = true; // Default for Context->DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsOrder

			if (Context->DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsAttribute)
			{
				bAscending = Context->VtxDirCompGetter->SafeGet(Edge.Start, Edge.Start) < Context->VtxDirCompGetter->SafeGet(Edge.End, Edge.End);
			}
			else if (Context->DirectionMethod == EPCGExEdgeDirectionMethod::EdgeDotAttribute)
			{
				const FVector CounterDir = Context->EdgeDirCompGetter->SafeGet(Edge.EdgeIndex, FVector::UpVector);
				const FVector StartEndDir = (EndPoint.Transform.GetLocation() - StartPoint.Transform.GetLocation()).GetSafeNormal();
				const FVector EndStartDir = (StartPoint.Transform.GetLocation() - EndPoint.Transform.GetLocation()).GetSafeNormal();
				bAscending = CounterDir.Dot(StartEndDir) < CounterDir.Dot(EndStartDir);
			}
			else if (Context->DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsIndices)
			{
				bAscending = Edge.Start < Edge.End;
			}

			const bool bInvert = bAscending != Context->bAscendingDesired;

			const PCGEx::FPointRef Target = Context->CurrentEdges->GetOutPointRef(Edge.PointIndex);
			if (Context->MetadataBlender)
			{
				Context->MetadataBlender->PrepareForBlending(Target);
				if (bInvert)
				{
					Context->MetadataBlender->Blend(Target, Context->CurrentIO->GetInPointRef(Edge.End), Target, Context->StartWeight);
					Context->MetadataBlender->Blend(Target, Context->CurrentIO->GetInPointRef(Edge.Start), Target, Context->EndWeight);
				}
				else
				{
					Context->MetadataBlender->Blend(Target, Context->CurrentIO->GetInPointRef(Edge.Start), Target, Context->StartWeight);
					Context->MetadataBlender->Blend(Target, Context->CurrentIO->GetInPointRef(Edge.End), Target, Context->EndWeight);
				}

				Context->MetadataBlender->CompleteBlending(Target, 2);
			}

			if (bInvert)
			{
				PCGEX_OUTPUT_VALUE(EdgeDirection, Edge.PointIndex, (DirTo - DirFrom).GetSafeNormal());
				PCGEX_OUTPUT_VALUE(EdgeLength, Edge.PointIndex, FVector::Distance(DirFrom, DirTo));
				if (Context->bWriteEdgePosition) { const_cast<FPCGPoint*>(Target.Point)->Transform.SetLocation(FMath::Lerp(DirFrom, DirTo, Context->EdgePositionLerp)); }
			}
			else
			{
				PCGEX_OUTPUT_VALUE(EdgeDirection, Edge.PointIndex, (DirFrom - DirTo).GetSafeNormal());
				PCGEX_OUTPUT_VALUE(EdgeLength, Edge.PointIndex, FVector::Distance(DirFrom, DirTo));
				if (Context->bWriteEdgePosition) { const_cast<FPCGPoint*>(Target.Point)->Transform.SetLocation(FMath::Lerp(DirTo, DirFrom, Context->EdgePositionLerp)); }
			}
		};

		if (!Context->Process(ProcessEdge, Context->CurrentCluster->Edges.Num())) { return false; }

		PCGEX_OUTPUT_WRITE(EdgeLength, double)
		PCGEX_OUTPUT_WRITE(EdgeDirection, FVector)
		if (Context->MetadataBlender) { Context->MetadataBlender->Write(); }

		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsDone()) { Context->OutputPointsAndEdges(); }

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
