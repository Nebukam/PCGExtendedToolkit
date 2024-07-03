// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "..\..\Public\Misc\PCGExMetaFilter.h"

#include "Helpers/PCGHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExMetaFilterElement"
#define PCGEX_NAMESPACE MetaFilter

PCGExData::EInit UPCGExMetaFilterSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(MetaFilter)

FPCGExMetaFilterContext::~FPCGExMetaFilterContext()
{
}

bool FPCGExMetaFilterElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(MetaFilter)

	PCGEX_FWD(Filters)
	Context->Filters.Init();

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
			Context->CurrentIO->InitializeOutput(PCGExData::EInit::Forward);
			Context->Filters.Filter(Context->CurrentIO->Tags);
		}
	}
	else
	{
		while (Context->AdvancePointsIO())
		{
			// TODO : Check if any attribute is affected first, and forward instead of duplicate if not.
			Context->CurrentIO->InitializeOutput(PCGExData::EInit::DuplicateInput);
			Context->Filters.Filter(Context->CurrentIO);
		}
	}

	Context->OutputMainPoints();
	Context->Done();

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
