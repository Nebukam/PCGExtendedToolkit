// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Filters/Collections/PCGExEntryCountFilter.h"


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
		if (!PCGExData::Helpers::TryReadDataValue(IO->GetContext(), IO->GetIn(), TypedFilterFactory->Config.OperandBAttr, B, PCGEX_QUIET_HANDLING)) { PCGEX_QUIET_HANDLING_RET }
	}

	return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, IO->GetNum(), B, TypedFilterFactory->Config.Tolerance);
}

PCGEX_CREATE_FILTER_FACTORY(EntryCount)

#if WITH_EDITOR
FString UPCGExEntryCountFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = TEXT("Entry Count ") + PCGExCompare::ToString(Config.Comparison);
	if (Config.CompareAgainst == EPCGExInputValueType::Constant) { DisplayName += FString::Printf(TEXT("%d"), Config.OperandB); }
	else { DisplayName += PCGExMetaHelpers::GetSelectorDisplayName(Config.OperandBAttr); }
	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
