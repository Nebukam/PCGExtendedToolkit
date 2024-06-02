// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/PCGExUberFilter.h"

#include "Data/PCGExData.h"
#include "Data/PCGExDataFilter.h"

#define LOCTEXT_NAMESPACE "PCGExUberFilter"
#define PCGEX_NAMESPACE UberFilter

TArray<FPCGPinProperties> UPCGExUberFilterSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExDataFilter::SourceFiltersLabel, "Filters.", Required, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExUberFilterSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExDataFilter::OutputInsideFiltersLabel, "Points that passed the filters.", Required, {})
	PCGEX_PIN_POINTS(PCGExDataFilter::OutputOutsideFiltersLabel, "Points that didn't pass the filters.", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExUberFilterSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FPCGExUberFilterContext::~FPCGExUberFilterContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_DELETE(FilterManager)
	PCGEX_DELETE(Inside)
	PCGEX_DELETE(Outside)
}

PCGEX_INITIALIZE_ELEMENT(UberFilter)

bool FPCGExUberFilterElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(UberFilter)

	Context->Inside = new PCGExData::FPointIOCollection();
	Context->Inside->DefaultOutputLabel = PCGExDataFilter::OutputInsideFiltersLabel;

	Context->Outside = new PCGExData::FPointIOCollection();
	Context->Outside->DefaultOutputLabel = PCGExDataFilter::OutputOutsideFiltersLabel;

	return PCGExDataFilter::GetInputFactories(Context, PCGExDataFilter::SourceFiltersLabel, Context->Factories, {PCGExFactories::EType::Filter});
}

bool FPCGExUberFilterElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExUberFilterElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(UberFilter)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->FilterManager)

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			Context->FilterManager = new PCGExDataFilter::TEarlyExitFilterManager(Context->CurrentIO);
			Context->FilterManager->Register<UPCGExFilterFactoryBase>(Context, Context->Factories, Context->CurrentIO);

			if (!Context->FilterManager->bValid)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points could not be filtered, likely due to missing attributes."));
				return false;
			}

			Context->CurrentIO->CreateInKeys();
			Context->FilterManager->PrepareForTesting();

			Context->SetState(PCGExMT::State_ProcessingPoints);
		}
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto ProcessPoint = [&](const int32 Index, const PCGExData::FPointIO& PointIO) { Context->FilterManager->Test(Index); };

		if (!Context->ProcessCurrentPoints(ProcessPoint)) { return false; }

		const TArray<FPCGPoint>& InPoints = Context->CurrentIO->GetIn()->GetPoints();

		TArray<FPCGPoint>& InsidePoints = Context->Inside->Emplace_GetRef(*Context->CurrentIO, PCGExData::EInit::NewOutput).GetOut()->GetMutablePoints();
		TArray<FPCGPoint>& OutsidePoints = Context->Outside->Emplace_GetRef(*Context->CurrentIO, PCGExData::EInit::NewOutput).GetOut()->GetMutablePoints();

		int32 Index = 0;

		if (Settings->bSwap)
		{
			for (const FPCGPoint& Point : InPoints)
			{
				if (!Context->FilterManager->Results[Index++]) { InsidePoints.Emplace(Point); }
				else { OutsidePoints.Emplace(Point); }
			}
		}
		else
		{
			for (const FPCGPoint& Point : InPoints)
			{
				if (Context->FilterManager->Results[Index++]) { InsidePoints.Emplace(Point); }
				else { OutsidePoints.Emplace(Point); }
			}
		}

		PCGEX_TRIM(OutsidePoints)
		PCGEX_TRIM(InsidePoints)

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->Inside->OutputTo(Context);
		Context->Outside->OutputTo(Context);
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
