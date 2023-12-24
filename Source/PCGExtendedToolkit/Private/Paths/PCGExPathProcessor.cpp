// Copyright Timothé Lapetite 2023
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

void FPCGExPathProcessorContext::Done()
{
	FPCGExPointsProcessorContext::Done();

#if WITH_EDITOR
	const UPCGExPathProcessorSettings* Settings = GetInputSettings<UPCGExPathProcessorSettings>();
	check(Settings);

	if (DebugEdgeData)
	{
		if (PCGExDebug::NotifyExecute(this)) { DebugEdgeData->Draw(World, Settings->DebugEdgeSettings); }
	}
#endif
}

PCGEX_INITIALIZE_CONTEXT(PathProcessor)

bool FPCGExPathProcessorElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathProcessor)

#if WITH_EDITOR
	if (Settings->IsDebugEnabled()) { Context->DebugEdgeData = new PCGExGraph::FDebugEdgeData(); }
#endif

	return true;
}


#undef LOCTEXT_NAMESPACE
