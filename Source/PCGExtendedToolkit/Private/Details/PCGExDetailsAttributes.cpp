// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Details/PCGExDetailsAttributes.h"

#include "PCGEx.h"
#include "PCGExContext.h"
#include "PCGModule.h"
#include "Details/PCGExMacros.h"


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
