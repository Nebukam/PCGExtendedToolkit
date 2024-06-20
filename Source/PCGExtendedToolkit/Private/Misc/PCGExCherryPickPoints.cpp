// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExCherryPickPoints.h"

#define LOCTEXT_NAMESPACE "PCGExCherryPickPointsElement"
#define PCGEX_NAMESPACE CherryPickPoints

PCGExData::EInit UPCGExCherryPickPointsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(CherryPickPoints)

TArray<FPCGPinProperties> UPCGExCherryPickPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (IndicesSource == EPCGExCherryPickSource::Target) { PCGEX_PIN_POINT(PCGEx::SourceTargetsLabel, "TBD", Required, {}) }
	return PinProperties;
}

FPCGExCherryPickPointsContext::~FPCGExCherryPickPointsContext()
{
}

bool FPCGExCherryPickPointsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CherryPickPoints)


	return true;
}

bool FPCGExCherryPickPointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCherryPickPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(CherryPickPoints)

	if (!Boot(Context)) { return true; }
	/*
		Context->MainPoints->ForEach(
			[&](const PCGExData::FPointIO& PointIO, const int32 Index)
			{
				for (const FName& Name : Names) { PointIO.GetOut()->Metadata->DeleteAttribute(Name); }
			});
	*/
	Context->OutputMainPoints();
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
