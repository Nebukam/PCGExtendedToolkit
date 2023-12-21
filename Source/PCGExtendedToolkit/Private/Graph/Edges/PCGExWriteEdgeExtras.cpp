// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExWriteEdgeExtras.h"

#include "Data/Blending/PCGExMetadataBlender.h"
#include "Data/Blending/PCGExPropertiesBlender.h"
#include "Graph/Edges/Promoting/PCGExEdgePromoteToPoint.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"
#define PCGEX_NAMESPACE WriteEdgeExtras

UPCGExWriteEdgeExtrasSettings::UPCGExWriteEdgeExtrasSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EInit UPCGExWriteEdgeExtrasSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGElementPtr UPCGExWriteEdgeExtrasSettings::CreateElement() const { return MakeShared<FPCGExWriteEdgeExtrasElement>(); }

FPCGExWriteEdgeExtrasContext::~FPCGExWriteEdgeExtrasContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_WRITEEDGEEXTRA_FOREACH(PCGEX_OUTPUT_DELETE)

	PCGEX_DELETE(MetadataBlender)
}

PCGEX_INITIALIZE_CONTEXT(WriteEdgeExtras)

bool FPCGExWriteEdgeExtrasElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteEdgeExtras)

	Context->MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(&Settings->BlendingSettings));

	PCGEX_WRITEEDGEEXTRA_FOREACH(PCGEX_OUTPUT_FWD)
	PCGEX_WRITEEDGEEXTRA_FOREACH(PCGEX_OUTPUT_VALIDATE_NAME)

	return true;
}

bool FPCGExWriteEdgeExtrasElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteEdgeExtrasElement::Execute);

	PCGEX_CONTEXT(WriteEdgeExtras)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvanceAndBindPointsIO()) { Context->Done(); }
		else
		{
			if (!Context->BoundEdges->IsValid())
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no bound edges."));
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
			}
			else
			{
				Context->SetState(PCGExGraph::State_ReadyForNextEdges);
			}
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		if (!Context->AdvanceEdges()) { Context->SetState(PCGExMT::State_ReadyForNextPoints); }
		else
		{
			if (Context->CurrentMesh->HasInvalidEdges())
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input edges are invalid. They will be omitted from the calculation."));
			}

			PCGExData::FPointIO& PointIO = *Context->CurrentEdges;
			PCGEX_WRITEEDGEEXTRA_FOREACH(PCGEX_OUTPUT_ACCESSOR_INIT)

			Context->MetadataBlender->PrepareForData(PointIO, *Context->CurrentIO);

			Context->GetAsyncManager()->Start<FWriteExtrasTask>(-1, &PointIO);

			Context->SetAsyncState(PCGExGraph::State_ProcessingEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		if (Context->IsAsyncWorkComplete())
		{
			Context->CurrentEdges->Cleanup();
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsDone()) { Context->OutputPointsAndEdges(); }

	return Context->IsDone();
}

bool FWriteExtrasTask::ExecuteTask()
{
	const FPCGExWriteEdgeExtrasContext* Context = Manager->GetContext<FPCGExWriteEdgeExtrasContext>();

	const TArray<PCGExMesh::FIndexedEdge>& Edges = Context->CurrentMesh->Edges;
	for (const PCGExMesh::FIndexedEdge& Edge : Edges)
	{
		Context->MetadataBlender->PrepareForBlending(Edge.Index);
		Context->MetadataBlender->Blend(Edge.Index, Edge.Start, Edge.Index, 0.5);
		Context->MetadataBlender->Blend(Edge.Index, Edge.End, Edge.Index, 0.5);
		Context->MetadataBlender->CompleteBlending(Edge.Index, 2);

		const FPCGPoint& StartPoint = Context->CurrentIO->GetInPoint(Edge.Start);
		const FPCGPoint& EndPoint = Context->CurrentIO->GetInPoint(Edge.End);

		PCGEX_OUTPUT_VALUE(EdgeLength, Edge.Index, FVector::Distance(StartPoint.Transform.GetLocation(), EndPoint.Transform.GetLocation()));
	}

	PCGEX_OUTPUT_WRITE(EdgeLength, double)
	Context->MetadataBlender->Write();

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
