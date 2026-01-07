// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Details/PCGExAttributesDetails.h"

#include "Core/PCGExContext.h"
#include "PCGModule.h"
#include "PCGExCoreMacros.h"
#include "Helpers/PCGExMetaHelpers.h"
#include "Metadata/PCGMetadata.h"

#pragma region legacy


FPCGExInputConfig::FPCGExInputConfig(const FPCGAttributePropertyInputSelector& InSelector)
{
	Selector.ImportFromOtherSelector(InSelector);
}

FPCGExInputConfig::FPCGExInputConfig(const FPCGExInputConfig& Other)
	: Attribute(Other.Attribute)
{
	Selector.ImportFromOtherSelector(Other.Selector);
}

FPCGExInputConfig::FPCGExInputConfig(const FName InName)
{
	Selector.Update(InName.ToString());
}

#if WITH_EDITOR
FString FPCGExInputConfig::GetDisplayName() const { return GetName().ToString(); }

void FPCGExInputConfig::UpdateUserFacingInfos() { TitlePropertyName = GetDisplayName(); }
#endif

bool FPCGExInputConfig::Validate(const UPCGData* InData)
{
	Selector = Selector.CopyAndFixLast(InData);
	if (GetSelection() == EPCGAttributePropertySelection::Attribute)
	{
		Attribute = Selector.IsValid() ? InData->Metadata->GetMutableAttribute(PCGExMetaHelpers::GetAttributeIdentifier(Selector, InData)) : nullptr;
		UnderlyingType = Attribute ? Attribute->GetTypeId() : static_cast<int16>(EPCGMetadataTypes::Unknown);
		return Attribute != nullptr;
	}

	if (Selector.IsValid() && Selector.GetSelection() == EPCGAttributePropertySelection::Property)
	{
		UnderlyingType = static_cast<int16>(PCGExMetaHelpers::GetPropertyType(Selector.GetPointProperty()));
		return true;
	}

	return false;
}

#pragma endregion

bool FPCGExAttributeSourceToTargetDetails::ValidateNames(FPCGExContext* InContext) const
{
	PCGEX_VALIDATE_NAME_CONSUMABLE_C(InContext, Source)
	if (WantsRemappedOutput()) { PCGEX_VALIDATE_NAME_C(InContext, Target) }
	return true;
}

bool FPCGExAttributeSourceToTargetDetails::ValidateNamesOrProperties(FPCGExContext* InContext) const
{
	FPCGAttributePropertyInputSelector Selector;
	Selector.Update(Source.ToString());
	if (Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
	{
		PCGEX_VALIDATE_NAME_CONSUMABLE_C(InContext, Source)
	}

	if (WantsRemappedOutput())
	{
		Selector.Update(Target.ToString());
		if (Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			PCGEX_VALIDATE_NAME_C(InContext, Target)
		}
	}
	return true;
}

FName FPCGExAttributeSourceToTargetDetails::GetOutputName() const
{
	return bOutputToDifferentName ? Target : Source;
}

FPCGAttributePropertyInputSelector FPCGExAttributeSourceToTargetDetails::GetSourceSelector() const
{
	FPCGAttributePropertyInputSelector Selector;
	Selector.Update(Source.ToString());
	return Selector;
}

FPCGAttributePropertyInputSelector FPCGExAttributeSourceToTargetDetails::GetTargetSelector() const
{
	FPCGAttributePropertyInputSelector Selector;
	Selector.Update(GetOutputName().ToString());
	return Selector;
}

bool FPCGExAttributeSourceToTargetList::ValidateNames(FPCGExContext* InContext) const
{
	for (const FPCGExAttributeSourceToTargetDetails& Entry : Attributes) { if (!Entry.ValidateNames(InContext)) { return false; } }
	return true;
}

void FPCGExAttributeSourceToTargetList::GetSources(TArray<FName>& OutNames) const
{
	OutNames.Reserve(OutNames.Num() + Attributes.Num());
	for (const FPCGExAttributeSourceToTargetDetails& Entry : Attributes) { OutNames.Add(Entry.Source); }
}
