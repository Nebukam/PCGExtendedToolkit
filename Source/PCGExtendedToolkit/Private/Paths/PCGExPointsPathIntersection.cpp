// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPointsPathIntersection.h"

#define LOCTEXT_NAMESPACE "PCGExPointsPathIntersectionElement"
#define PCGEX_NAMESPACE PointsPathIntersection

TArray<FPCGPinProperties> UPCGExPointsPathIntersectionSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceTargetsLabel, "Intersection targets", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExPointsPathIntersectionSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

PCGEX_INITIALIZE_ELEMENT(PointsPathIntersection)

FPCGExPointsPathIntersectionContext::~FPCGExPointsPathIntersectionContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExPointsPathIntersectionElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PointsPathIntersection)

	return true;
}

bool FPCGExPointsPathIntersectionElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPointsPathIntersectionElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PointsPathIntersection)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->Done();
			return false;
		}
	}

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

bool FPCGExPointsPathIntersectionTask::ExecuteTask()
{
	const FPCGExPointsPathIntersectionContext* Context = Manager->GetContext<FPCGExPointsPathIntersectionContext>();
	PCGEX_SETTINGS(PointsPathIntersection)

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
