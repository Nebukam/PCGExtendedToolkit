// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExDeleteAttributes.h"

#define LOCTEXT_NAMESPACE "PCGExDeleteAttributesElement"
#define PCGEX_NAMESPACE DeleteAttributes

PCGExData::EInit UPCGExDeleteAttributesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(DeleteAttributes)

FPCGExDeleteAttributesContext::~FPCGExDeleteAttributesContext()
{
}

bool FPCGExDeleteAttributesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(DeleteAttributes)

	if (Settings->AttributeNames.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Attribute list is empty."));
		return false;
	}

	return true;
}

bool FPCGExDeleteAttributesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDeleteAttributesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(DeleteAttributes)

	if (!Boot(Context)) { return true; }

	TArray<FName> Names = Settings->AttributeNames.Array();
	Context->MainPoints->ForEach(
		[&](const PCGExData::FPointIO& PointIO, const int32 Index)
		{
			for (const FName& Name : Names) { PointIO.GetOut()->Metadata->DeleteAttribute(Name); }
		});

	Context->OutputMainPoints();
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
