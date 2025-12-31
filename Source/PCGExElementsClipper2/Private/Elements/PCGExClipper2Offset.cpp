// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExClipper2Offset.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"

#define LOCTEXT_NAMESPACE "PCGExClipper2OffsetElement"
#define PCGEX_NAMESPACE Clipper2Offset

PCGEX_INITIALIZE_ELEMENT(Clipper2Offset)

FPCGExGeo2DProjectionDetails UPCGExClipper2OffsetSettings::GetProjectionDetails() const
{
	return ProjectionDetails;
}

bool FPCGExClipper2OffsetElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClipper2ProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Clipper2Offset)

	return true;
}

bool FPCGExClipper2OffsetElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExClipper2OffsetElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(Clipper2Offset)

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
