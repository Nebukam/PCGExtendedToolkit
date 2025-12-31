// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExClipper2Boolean.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"

#define LOCTEXT_NAMESPACE "PCGExClipper2BooleanElement"
#define PCGEX_NAMESPACE Clipper2Boolean

PCGEX_INITIALIZE_ELEMENT(Clipper2Boolean)

FPCGExGeo2DProjectionDetails UPCGExClipper2BooleanSettings::GetProjectionDetails() const
{
	return ProjectionDetails;
}

bool FPCGExClipper2BooleanElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClipper2ProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Clipper2Boolean)

	return true;
}

bool FPCGExClipper2BooleanElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExClipper2BooleanElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(Clipper2Boolean)

	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		// TODO : Offset main paths
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	PCGEX_OUTPUT_VALID_PATHS(MainPoints)

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
