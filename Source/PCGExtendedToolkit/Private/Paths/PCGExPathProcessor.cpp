// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathProcessor.h"

#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExPathProcessorElement"

UPCGExPathProcessorSettings::UPCGExPathProcessorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EInit UPCGExPathProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FName UPCGExPathProcessorSettings::GetMainInputLabel() const { return PCGExGraph::SourcePathsLabel; }
FName UPCGExPathProcessorSettings::GetMainOutputLabel() const { return PCGExGraph::OutputPathsLabel; }

PCGEX_INITIALIZE_CONTEXT(PathProcessor)


#undef LOCTEXT_NAMESPACE
