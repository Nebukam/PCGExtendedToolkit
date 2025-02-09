// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/CollectionFilters/PCGExAttributeCheckFilter.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

TSharedPtr<PCGExPointFilter::FFilter> UPCGExAttributeCheckFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FAttributeCheckFilter>(this);
}

bool PCGExPointFilter::FAttributeCheckFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO) const
{
	const TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(IO->GetIn()->Metadata);

	bool bResult = false;

	if (TypedFilterFactory->Config.bDoCheckType)
	{
		for (const PCGEx::FAttributeIdentity& Identity : Infos->Identities)
		{
			const FString Str = Identity.Name.ToString();
			bool bMatches = false;

			switch (TypedFilterFactory->Config.Match)
			{
			case EPCGExStringMatchMode::Equals:
				if (Str == TypedFilterFactory->Config.AttributeName) { bMatches = true; }
				break;
			case EPCGExStringMatchMode::Contains:
				if (Str.Contains(TypedFilterFactory->Config.AttributeName)) { bMatches = true; }
				break;
			case EPCGExStringMatchMode::StartsWith:
				if (Str.StartsWith(TypedFilterFactory->Config.AttributeName)) { bMatches = true; }
				break;
			case EPCGExStringMatchMode::EndsWith:
				if (Str.EndsWith(TypedFilterFactory->Config.AttributeName)) { bMatches = true; }
				break;
			}

			if (bMatches && Identity.UnderlyingType == TypedFilterFactory->Config.Type)
			{
				bResult = true;
			}
		}
	}
	else
	{
		for (const PCGEx::FAttributeIdentity& Identity : Infos->Identities)
		{
			const FString Str = Identity.Name.ToString();
			bool bMatches = false;

			switch (TypedFilterFactory->Config.Match)
			{
			case EPCGExStringMatchMode::Equals:
				if (Str == TypedFilterFactory->Config.AttributeName) { bMatches = true; }
				break;
			case EPCGExStringMatchMode::Contains:
				if (Str.Contains(TypedFilterFactory->Config.AttributeName)) { bMatches = true; }
				break;
			case EPCGExStringMatchMode::StartsWith:
				if (Str.StartsWith(TypedFilterFactory->Config.AttributeName)) { bMatches = true; }
				break;
			case EPCGExStringMatchMode::EndsWith:
				if (Str.EndsWith(TypedFilterFactory->Config.AttributeName)) { bMatches = true; }
				break;
			}

			if (bMatches)
			{
				bResult = true;
				break;
			}
		}
	}

	return TypedFilterFactory->Config.bInvert ? !bResult : bResult;
}

PCGEX_CREATE_FILTER_FACTORY(AttributeCheck)

#if WITH_EDITOR
FString UPCGExAttributeCheckFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = TEXT("Attribute ") + PCGExCompare::ToString(Config.Match);
	DisplayName += FString::Printf(TEXT(" \"%s\""), *Config.AttributeName);
	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
