// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExMetaFilter.h"

#include "Helpers/PCGHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExMetaFilterElement"
#define PCGEX_NAMESPACE MetaFilter

PCGExData::EInit UPCGExMetaFilterSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

TArray<FPCGPinProperties> UPCGExMetaFilterSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExPointFilter::OutputInsideFiltersLabel, "Collections that passed the tests.", Required, {})
	PCGEX_PIN_POINTS(PCGExPointFilter::OutputOutsideFiltersLabel, "Collections that didn't pass the tests.", Required, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(MetaFilter)

FPCGExMetaFilterContext::~FPCGExMetaFilterContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(Inside)
	PCGEX_DELETE(Outside)
}

bool FPCGExMetaFilterElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(MetaFilter)

	PCGEX_FWD(Filters)
	Context->Filters.Init();

	Context->Inside = new PCGExData::FPointIOCollection(Context);
	Context->Outside = new PCGExData::FPointIOCollection(Context);

	Context->Inside->DefaultOutputLabel = PCGExPointFilter::OutputInsideFiltersLabel;
	Context->Outside->DefaultOutputLabel = PCGExPointFilter::OutputOutsideFiltersLabel;

	if (Settings->bSwap)
	{
		Context->Inside->DefaultOutputLabel = PCGExPointFilter::OutputOutsideFiltersLabel;
		Context->Outside->DefaultOutputLabel = PCGExPointFilter::OutputInsideFiltersLabel;
	}

	return true;
}

bool FPCGExMetaFilterElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMetaFilterElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(MetaFilter)

	if (!Boot(Context)) { return true; }

	if (Context->Filters.Attributes.FilterMode == EPCGExAttributeFilter::All)
	{
		while (Context->AdvancePointsIO())
		{
			PCGExData::FPointIOCollection* Target = Context->Filters.Test(Context->CurrentIO->Tags) ? Context->Inside : Context->Outside;
			Target->Emplace_GetRef(Context->CurrentIO, PCGExData::EInit::Forward);
		}
	}
	else
	{
		while (Context->AdvancePointsIO())
		{
			PCGExData::FPointIOCollection* Target = Context->Filters.Test(Context->CurrentIO) ? Context->Inside : Context->Outside;
			Target->Emplace_GetRef(Context->CurrentIO, PCGExData::EInit::Forward);
		}
	}

	Context->Inside->OutputToContext();
	Context->Outside->OutputToContext();
	Context->Done();

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
