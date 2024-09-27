// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExMetaCleanup.h"

#define LOCTEXT_NAMESPACE "PCGExMetaCleanupElement"
#define PCGEX_NAMESPACE MetaCleanup

PCGExData::EInit UPCGExMetaCleanupSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(MetaCleanup)

bool FPCGExMetaCleanupElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(MetaCleanup)

	PCGEX_FWD(Filters)
	Context->Filters.Init();

	return true;
}

bool FPCGExMetaCleanupElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMetaCleanupElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(MetaCleanup)

	if (!Boot(Context)) { return true; }

	if (Context->Filters.Attributes.FilterMode == EPCGExAttributeFilter::All)
	{
		while (Context->AdvancePointsIO())
		{
			Context->CurrentIO->InitializeOutput(PCGExData::EInit::Forward);
			Context->Filters.Filter(Context->CurrentIO->Tags.Get());
		}
	}
	else
	{
		while (Context->AdvancePointsIO())
		{
			// TODO : Check if any attribute is affected first, and forward instead of duplicate if not.
			Context->CurrentIO->InitializeOutput(PCGExData::EInit::DuplicateInput);
			Context->Filters.Filter(Context->CurrentIO.Get());
		}
	}

	Context->MainPoints->OutputToContext();
	Context->Done();

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
