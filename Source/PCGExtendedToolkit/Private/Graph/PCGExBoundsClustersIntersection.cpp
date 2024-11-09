// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBoundsClustersIntersection.h"


#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

TArray<FPCGPinProperties> UPCGExBoundsClustersIntersectionSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceBoundsLabel, "Intersection points (bounds)", Required, {})
	return PinProperties;
}

PCGExData::EIOInit UPCGExBoundsClustersIntersectionSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::DuplicateInput; }
PCGExData::EIOInit UPCGExBoundsClustersIntersectionSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::DuplicateInput; }

#pragma endregion

PCGEX_INITIALIZE_ELEMENT(BoundsClustersIntersection)

bool FPCGExBoundsClustersIntersectionElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BoundsClustersIntersection)

	const TSharedPtr<PCGExData::FPointIO> BoundsIO = PCGExData::TryGetSingleInput(InContext, PCGEx::SourceBoundsLabel, true);
	if (!BoundsIO) { return false; }

	Context->BoundsDataFacade = MakeShared<PCGExData::FFacade>(BoundsIO.ToSharedRef());

	return true;
}

bool FPCGExBoundsClustersIntersectionElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBoundsClustersIntersectionElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BoundsClustersIntersection)
	PCGEX_EXECUTION_CHECK

	PCGE_LOG(Error, GraphAndLog, FTEXT("NOT IMPLEMENTED YET"));
	return true;

	/*
	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGEx::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGEx::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges) { return false; }

			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		while (Context->AdvanceEdges(false))
		{
		}

		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		PCGEX_ASYNC_WAIT

		Context->SetState(PCGEx::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->MainPoints->OutputToContext();
	}

	return Context->CompleteTaskExecution();
	*/
}

#undef LOCTEXT_NAMESPACE
