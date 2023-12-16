// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExWriteEdgeExtras.h"

#include "Graph/Promotions/PCGExEdgePromoteToPath.h"
#include "Graph/Promotions/PCGExEdgePromoteToPoint.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"
#define PCGEX_NAMESPACE WriteEdgeExtras

UPCGExWriteEdgeExtrasSettings::UPCGExWriteEdgeExtrasSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EInit UPCGExWriteEdgeExtrasSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGElementPtr UPCGExWriteEdgeExtrasSettings::CreateElement() const { return MakeShared<FPCGExWriteEdgeExtrasElement>(); }

PCGEX_INITIALIZE_CONTEXT(WriteEdgeExtras)

bool FPCGExWriteEdgeExtrasElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteEdgeExtras)

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
		if (!Context->AdvancePointsIO()) { Context->Done(); }
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
		Context->SetState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		auto Initialize = [&](const PCGExData::FPointIO& PointIO)
		{
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint))
		{
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
