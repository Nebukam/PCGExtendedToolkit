// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExMatchAndRemoveTags.h"

#define LOCTEXT_NAMESPACE "PCGExMatchAndRemoveTagsElement"
#define PCGEX_NAMESPACE MatchAndRemoveTags

PCGExData::EInit UPCGExMatchAndRemoveTagsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::Forward; }

PCGEX_INITIALIZE_ELEMENT(MatchAndRemoveTags)

FPCGExMatchAndRemoveTagsContext::~FPCGExMatchAndRemoveTagsContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExMatchAndRemoveTagsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(MatchAndRemoveTags)

	return true;
}

bool FPCGExMatchAndRemoveTagsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMatchAndRemoveTagsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(MatchAndRemoveTags)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		switch (Settings->Mode)
		{
		default: ;
		case EPCGExStringMatchMode::Contains:
			for (const PCGExData::FPointIO* IO : Context->MainPoints->Pairs) { IO->Tags->RemoveContains(Settings->Matches); }
			break;
		case EPCGExStringMatchMode::StartsWith:
			for (const PCGExData::FPointIO* IO : Context->MainPoints->Pairs) { IO->Tags->RemoveStartsWith(Settings->Matches); }
			break;
		case EPCGExStringMatchMode::EndsWith:
			for (const PCGExData::FPointIO* IO : Context->MainPoints->Pairs) { IO->Tags->RemoveEndsWith(Settings->Matches); }
			break;
		}
	}

	Context->OutputMainPoints();
	Context->Done();

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
