// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathProcessor.h"

#define LOCTEXT_NAMESPACE "PCGExPathProcessorElement"

UPCGExPathProcessorSettings::UPCGExPathProcessorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EInit UPCGExPathProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FName UPCGExPathProcessorSettings::GetMainInputLabel() const { return PCGExGraph::SourcePathsLabel; }
FName UPCGExPathProcessorSettings::GetMainOutputLabel() const { return PCGExGraph::OutputPathsLabel; }

PCGEX_INITIALIZE_CONTEXT(PathProcessor)

bool FPCGExPathProcessorElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathProcessor)

	return true;
}


#undef LOCTEXT_NAMESPACE
