// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExConsolidateEdgeIslands.h"

#define LOCTEXT_NAMESPACE "PCGExConsolidateEdgeIslands"
#define PCGEX_NAMESPACE ConsolidateEdgeIslands

UPCGExConsolidateEdgeIslandsSettings::UPCGExConsolidateEdgeIslandsSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EInit UPCGExConsolidateEdgeIslandsSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGElementPtr UPCGExConsolidateEdgeIslandsSettings::CreateElement() const { return MakeShared<FPCGExConsolidateEdgeIslandsElement>(); }

PCGEX_INITIALIZE_CONTEXT(ConsolidateEdgeIslands)

bool FPCGExConsolidateEdgeIslandsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ConsolidateEdgeIslands)

	return true;
}

bool FPCGExConsolidateEdgeIslandsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExConsolidateEdgeIslandsElement::Execute);

	PCGEX_CONTEXT(ConsolidateEdgeIslands)

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
