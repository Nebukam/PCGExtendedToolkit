// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExClipper2Inflate.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"

#define LOCTEXT_NAMESPACE "PCGExClipper2InflateElement"
#define PCGEX_NAMESPACE Clipper2Inflate

PCGEX_INITIALIZE_ELEMENT(Clipper2Inflate)

FPCGExGeo2DProjectionDetails UPCGExClipper2InflateSettings::GetProjectionDetails() const
{
	return ProjectionDetails;
}

bool FPCGExClipper2InflateElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClipper2ProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Clipper2Inflate)

	return true;
}

bool FPCGExClipper2InflateElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExClipper2InflateElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(Clipper2Inflate)

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
