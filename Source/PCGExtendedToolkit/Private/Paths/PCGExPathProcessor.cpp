// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathProcessor.h"

#define LOCTEXT_NAMESPACE "PCGExPathProcessorElement"

UPCGExPathProcessorSettings::UPCGExPathProcessorSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EInit UPCGExPathProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_CONTEXT(PathProcessor)

bool FPCGExPathProcessorElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathProcessor)

	PCGEX_FWD(ClosedLoop)
	Context->ClosedLoop.Init();

	return true;
}


#undef LOCTEXT_NAMESPACE
