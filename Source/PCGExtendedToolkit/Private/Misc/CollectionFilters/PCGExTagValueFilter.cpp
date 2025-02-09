// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/CollectionFilters/PCGExTagValueFilter.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

TSharedPtr<PCGExPointFilter::FFilter> UPCGExTagValueFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FTagValueFilter>(this);
}

bool PCGExPointFilter::FTagValueFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO) const
{
	bool bResult = false;

	if (TArray<TSharedPtr<PCGExData::FTagValue>> TagValues;
		PCGExCompare::GetMatchingValueTags(IO->Tags, TypedFilterFactory->Config.Tag, TypedFilterFactory->Config.Match, TagValues))
	{
		bResult = TypedFilterFactory->Config.MultiMatch == EPCGExFilterGroupMode::AND;

		if (TypedFilterFactory->Config.ValueType == EPCGExComparisonDataType::Numeric)
		{
			double B = TypedFilterFactory->Config.NumericOperandB;

			for (const TSharedPtr<PCGExData::FTagValue>& TagValue : TagValues)
			{
				if (!PCGExCompare::Compare(TypedFilterFactory->Config.NumericComparison, TagValue, B, TypedFilterFactory->Config.Tolerance))
				{
					bResult = !bCollectionTestResult;
					break;
				}
			}
		}
		else
		{
			FString B = TypedFilterFactory->Config.StringOperandB;
			for (const TSharedPtr<PCGExData::FTagValue>& TagValue : TagValues)
			{
				if (!PCGExCompare::Compare(TypedFilterFactory->Config.StringComparison, TagValue, B))
				{
					bResult = !bCollectionTestResult;
					break;
				}
			}
		}
	}

	return TypedFilterFactory->Config.bInvert ? !bResult : bResult;
}

PCGEX_CREATE_FILTER_FACTORY(TagValue)

#if WITH_EDITOR
FString UPCGExTagValueFilterProviderSettings::GetDisplayName() const
{
	if (Config.ValueType == EPCGExComparisonDataType::Numeric)
	{
		FString DisplayName = Config.Tag + TEXT(" ") + PCGExCompare::ToString(Config.NumericComparison);
		DisplayName += FString::Printf(TEXT("%.1f"), Config.NumericOperandB);
		DisplayName += Config.MultiMatch == EPCGExFilterGroupMode::OR ? TEXT(" (OR)") : TEXT(" (AND)");
		return DisplayName;
	}
	else
	{
		FString DisplayName = Config.Tag + TEXT(" ") + PCGExCompare::ToString(Config.StringComparison);
		DisplayName += FString::Printf(TEXT(" %s"), *Config.StringOperandB);
		DisplayName += Config.MultiMatch == EPCGExFilterGroupMode::OR ? TEXT(" (OR)") : TEXT(" (AND)");
		return DisplayName;
	}
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
