// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/CollectionFilters/PCGExEntryCountFilter.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

TSharedPtr<PCGExPointFilter::FFilter> UPCGExEntryCountFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FEntryCountFilter>(this);
}

bool PCGExPointFilter::FEntryCountFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO) const
{
	return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, IO->GetNum(), TypedFilterFactory->Config.OperandB);
}

PCGEX_CREATE_FILTER_FACTORY(EntryCount)

#if WITH_EDITOR
FString UPCGExEntryCountFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = TEXT("Entry Count ") + PCGExCompare::ToString(Config.Comparison);
	DisplayName += FString::Printf(TEXT("%d"), Config.OperandB);
	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
