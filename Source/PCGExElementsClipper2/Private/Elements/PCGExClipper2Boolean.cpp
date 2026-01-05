// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExClipper2Boolean.h"

#include "Data/PCGExPointIO.h"

#define LOCTEXT_NAMESPACE "PCGExClipper2BooleanElement"
#define PCGEX_NAMESPACE Clipper2Boolean

PCGEX_INITIALIZE_ELEMENT(Clipper2Boolean)

bool UPCGExClipper2BooleanSettings::NeedsOperands() const
{
	return bUseOperandsPin || Operation == EPCGExClipper2BooleanOp::Difference;
}

FPCGExGeo2DProjectionDetails UPCGExClipper2BooleanSettings::GetProjectionDetails() const
{
	return ProjectionDetails;
}

void FPCGExClipper2BooleanContext::Process(const TArray<int32>& Subjects, const TArray<int32>* Operands)
{
	// TODO : Implement
	// Subjects vs Operands
	// Subjects path64 can be retrieve via AllOpData->Paths[Subjects[i]]
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
