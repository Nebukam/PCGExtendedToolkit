﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/CollectionFilters/PCGExEntryCountFilter.h"

#include "PCGExHelpers.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExPointIO.h"

#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

TSharedPtr<PCGExPointFilter::IFilter> UPCGExEntryCountFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FEntryCountFilter>(this);
}

bool PCGExPointFilter::FEntryCountFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
{
	int32 B = TypedFilterFactory->Config.OperandB;
	if (TypedFilterFactory->Config.CompareAgainst == EPCGExInputValueType::Attribute)
	{
		if (!PCGExDataHelpers::TryReadDataValue(IO->GetContext(), IO->GetIn(), TypedFilterFactory->Config.OperandBAttr, B))
		{
			switch (TypedFilterFactory->Config.MissingAttributeFallback)
			{
			case EPCGExFilterFallback::Pass:
				return true;
			case EPCGExFilterFallback::Fail:
				return false;
			}
		}
	}

	return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, IO->GetNum(), B, TypedFilterFactory->Config.Tolerance);
}

PCGEX_CREATE_FILTER_FACTORY(EntryCount)

#if WITH_EDITOR
FString UPCGExEntryCountFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = TEXT("Entry Count ") + PCGExCompare::ToString(Config.Comparison);
	if (Config.CompareAgainst == EPCGExInputValueType::Constant) { DisplayName += FString::Printf(TEXT("%d"), Config.OperandB); }
	else { DisplayName += PCGEx::GetSelectorDisplayName(Config.OperandBAttr); }
	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
