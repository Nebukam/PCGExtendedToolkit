// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExDeleteAttributes.h"

#include "Helpers/PCGHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExDeleteAttributesElement"
#define PCGEX_NAMESPACE DeleteAttributes

PCGExData::EInit UPCGExDeleteAttributesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(DeleteAttributes)

FPCGExDeleteAttributesContext::~FPCGExDeleteAttributesContext()
{
}

bool FPCGExDeleteAttributesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(DeleteAttributes)

	Context->Targets.Append(Settings->AttributeNames);

	const TArray<FString> FilterAttributes = PCGHelpers::GetStringArrayFromCommaSeparatedString(Settings->CommaSeparatedNames);
	for (const FString& FilterAttribute : FilterAttributes)
	{
		Context->Targets.Add(FName(*FilterAttribute));
	}

	return true;
}

bool FPCGExDeleteAttributesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDeleteAttributesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(DeleteAttributes)

	if (!Boot(Context)) { return true; }

	if (Settings->Mode == EPCGExDeleteFilter::Keep)
	{
		while (Context->AdvancePointsIO())
		{
			UPCGMetadata* Metadata = Context->CurrentIO->GetOut()->Metadata;
			PCGEx::FAttributesInfos* Infos = PCGEx::FAttributesInfos::Get(Metadata);
			for (const PCGEx::FAttributeIdentity& Identity : Infos->Identities)
			{
				if (!Context->Targets.Contains(Identity.Name)) { Metadata->DeleteAttribute(Identity.Name); }
			}

			PCGEX_DELETE(Infos)
		}
	}
	else
	{
		UPCGMetadata* Metadata = Context->CurrentIO->GetOut()->Metadata;
		while (Context->AdvancePointsIO())
		{
			for (const FName& Name : Context->Targets) { Metadata->DeleteAttribute(Name); }
		}
	}

	Context->OutputMainPoints();
	Context->Done();
	Context->ExecuteEnd();

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
