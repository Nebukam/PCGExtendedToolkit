// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPrunePath.h"

#include "Geometry/PCGExGeoPointBox.h"

#define LOCTEXT_NAMESPACE "PCGExPrunePathElement"
#define PCGEX_NAMESPACE PrunePath

TArray<FPCGPinProperties> UPCGExPrunePathSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceBoundsLabel, "Bounds", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExPrunePathSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

PCGEX_INITIALIZE_ELEMENT(PrunePath)

FPCGExPrunePathContext::~FPCGExPrunePathContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(BoxCloud)
}

bool FPCGExPrunePathElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PrunePath)

	const PCGExData::FPointIO* Targets = PCGExData::TryGetSingleInput(Context, PCGEx::SourceBoundsLabel, true);

	if (!Targets)
	{
		PCGEX_DELETE(Targets)
		return false;
	}

	Context->BoxCloud = new PCGExGeo::FPointBoxCloud(Targets->GetIn(), Settings->BoundsSource, Settings->InsideEpsilon);

	PCGEX_DELETE(Targets)

	return true;
}

bool FPCGExPrunePathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPrunePathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PrunePath)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		while (Context->AdvancePointsIO(false))
		{
			Context->GetAsyncManager()->Start<FPCGExPrunePathTask>(Context->CurrentIO->IOIndex, Context->CurrentIO);
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_ASYNC_WAIT

		Context->Done();
	}

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
	}

	return Context->TryComplete();
}

bool FPCGExPrunePathTask::ExecuteTask()
{
	const FPCGExPrunePathContext* Context = Manager->GetContext<FPCGExPrunePathContext>();
	PCGEX_SETTINGS(PrunePath)

	// TODO : Check against BoxCloud

	// TODO : Keep/Omit/Tag
	// We can do this here as instead of inside the main thread.

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
