// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExSplitPath.h"

#define LOCTEXT_NAMESPACE "PCGExSplitPathElement"
#define PCGEX_NAMESPACE SplitPath

TArray<FPCGPinProperties> UPCGExSplitPathSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceTargetsLabel, "Intersection targets", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExSplitPathSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FName UPCGExSplitPathSettings::GetPointFilterLabel() const { return FName("SplitConditions"); }
bool UPCGExSplitPathSettings::RequiresPointFilters() const { return true; }


PCGEX_INITIALIZE_ELEMENT(SplitPath)

FPCGExSplitPathContext::~FPCGExSplitPathContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(MainPaths)
}

bool FPCGExSplitPathContext::PrepareFiltersWithAdvance() const { return false; }

bool FPCGExSplitPathElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SplitPath)

	Context->MainPaths = new PCGExData::FPointIOCollection();
	Context->MainPaths->DefaultOutputLabel = Settings->GetMainOutputLabel();

	return true;
}

bool FPCGExSplitPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSplitPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SplitPath)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		while (Context->AdvancePointsIO())
		{
			Context->GetAsyncManager()->Start<FPCGExSplitPathTask>(Context->CurrentIO->IOIndex, Context->CurrentIO);
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC

		Context->Done();
	}

	if (Context->IsDone())
	{
		Context->MainPaths->OutputTo(Context);
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

bool FPCGExSplitPathTask::ExecuteTask()
{
	const FPCGExSplitPathContext* Context = Manager->GetContext<FPCGExSplitPathContext>();
	PCGEX_SETTINGS(SplitPath)

	PCGExDataFilter::TEarlyExitFilterManager* Splits = Context->CreatePointFilterManagerInstance(PointIO);

	if (!Splits->bValid)
	{
		Context->MainPaths->Emplace_GetRef(PointIO->GetIn(), PCGExData::EInit::Forward);
		return false;
	}

	const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();

	Splits->PrepareForTesting();
	if (Splits->RequiresPerPointPreparation())
	{
		for (int i = 0; i < InPoints.Num(); i++) { Splits->PrepareSingle(i); }
		Splits->PreparationComplete();
	}

	bool bAnySplit = false;
	for (const bool Result : Splits->Results)
	{
		if (Result)
		{
			bAnySplit = true;
			break;
		}
	}

	if (!bAnySplit)
	{
		// Forward input as-is, no alterations
		Context->MainPaths->Emplace_GetRef(PointIO->GetIn(), PCGExData::EInit::Forward);
		return true;
	}

	// TODO : Go through each point and split/create new partition at each split

	for (int i = 0; i < InPoints.Num(); i++)
	{
		const FPCGPoint& CurrentPoint = InPoints[i];

		// Note: Start and End can never split, but can be removed.
	}

	// Context->MainPaths->Emplace_GetRef(PointIO->GetIn(), PCGExData::EInit::DuplicateInput);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
