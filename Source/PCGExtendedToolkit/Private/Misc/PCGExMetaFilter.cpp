// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExMetaFilter.h"

#include "Helpers/PCGHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExMetaFilterElement"
#define PCGEX_NAMESPACE MetaFilter

PCGExData::EIOInit UPCGExMetaFilterSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

TArray<FPCGPinProperties> UPCGExMetaFilterSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExPointFilter::OutputInsideFiltersLabel, "Collections that passed the tests.", Required, {})
	PCGEX_PIN_POINTS(PCGExPointFilter::OutputOutsideFiltersLabel, "Collections that didn't pass the tests.", Required, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(MetaFilter)

bool FPCGExMetaFilterElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(MetaFilter)

	PCGEX_FWD(Filters)
	Context->Filters.Init();

	Context->Inside = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->Outside = MakeShared<PCGExData::FPointIOCollection>(Context);

	Context->Inside->OutputPin = PCGExPointFilter::OutputInsideFiltersLabel;
	Context->Outside->OutputPin = PCGExPointFilter::OutputOutsideFiltersLabel;

	if (Settings->bSwap)
	{
		Context->Inside->OutputPin = PCGExPointFilter::OutputOutsideFiltersLabel;
		Context->Outside->OutputPin = PCGExPointFilter::OutputInsideFiltersLabel;
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
			const TSharedPtr<PCGExData::FPointIOCollection> Target = Context->Filters.Test(Context->CurrentIO->Tags.Get()) ? Context->Inside : Context->Outside;
			Target->Emplace_GetRef(Context->CurrentIO, PCGExData::EIOInit::Forward);
		}
	}
	else
	{
		while (Context->AdvancePointsIO())
		{
			const TSharedPtr<PCGExData::FPointIOCollection> Target = Context->Filters.Test(Context->CurrentIO.Get()) ? Context->Inside : Context->Outside;
			Target->Emplace_GetRef(Context->CurrentIO, PCGExData::EIOInit::Forward);
		}
	}

	Context->Inside->StageOutputs();
	Context->Outside->StageOutputs();
	Context->Done();

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
