// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/CollectionFilters/PCGExAttributeCheckFilter.h"

#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

TSharedPtr<PCGExPointFilter::IFilter> UPCGExAttributeCheckFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FAttributeCheckFilter>(this);
}

bool PCGExPointFilter::FAttributeCheckFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
{
	const TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(IO->GetIn()->Metadata);

	bool bResult = false;

	const FPCGAttributeIdentifier Identifier = PCGEx::GetAttributeIdentifier(FName(TypedFilterFactory->Config.AttributeName), IO->GetIn());
	const FString IdentifierStr = Identifier.Name.ToString();

	if (TypedFilterFactory->Config.bDoCheckType)
	{
		for (const PCGEx::FAttributeIdentity& Identity : Infos->Identities)
		{
			const FString Str = Identity.Identifier.Name.ToString();
			bool bMatches = false;

			if (TypedFilterFactory->Config.Domain != EPCGExAttribtueDomainCheck::Any)
			{
				if (TypedFilterFactory->Config.Domain == EPCGExAttribtueDomainCheck::Data && !Identity.InDataDomain()) { continue; }
				if (TypedFilterFactory->Config.Domain == EPCGExAttribtueDomainCheck::Elements && Identity.InDataDomain()) { continue; }
				if (TypedFilterFactory->Config.Domain == EPCGExAttribtueDomainCheck::Match && Identity.Identifier.MetadataDomain != Identifier.MetadataDomain) { continue; }
			}

			switch (TypedFilterFactory->Config.Match)
			{
			case EPCGExStringMatchMode::Equals:
				if (Identity.Identifier.Name == Identifier.Name) { bMatches = true; }
				break;
			case EPCGExStringMatchMode::Contains:
				if (Str.Contains(IdentifierStr)) { bMatches = true; }
				break;
			case EPCGExStringMatchMode::StartsWith:
				if (Str.StartsWith(IdentifierStr)) { bMatches = true; }
				break;
			case EPCGExStringMatchMode::EndsWith:
				if (Str.EndsWith(IdentifierStr)) { bMatches = true; }
				break;
			}

			if (bMatches && Identity.UnderlyingType == TypedFilterFactory->Config.Type)
			{
				bResult = true;
				break;
			}
		}
	}
	else
	{
		for (const PCGEx::FAttributeIdentity& Identity : Infos->Identities)
		{
			const FString Str = Identity.Identifier.Name.ToString();
			bool bMatches = false;

			if (TypedFilterFactory->Config.Domain != EPCGExAttribtueDomainCheck::Any)
			{
				if (TypedFilterFactory->Config.Domain == EPCGExAttribtueDomainCheck::Data && Identity.InDataDomain()) { continue; }
				if (TypedFilterFactory->Config.Domain == EPCGExAttribtueDomainCheck::Elements && Identity.InDataDomain()) { continue; }
				if (TypedFilterFactory->Config.Domain == EPCGExAttribtueDomainCheck::Match && Identity.Identifier.MetadataDomain != Identifier.MetadataDomain) { continue; }
			}

			switch (TypedFilterFactory->Config.Match)
			{
			case EPCGExStringMatchMode::Equals:
				if (Str == IdentifierStr) { bMatches = true; }
				break;
			case EPCGExStringMatchMode::Contains:
				if (Str.Contains(IdentifierStr)) { bMatches = true; }
				break;
			case EPCGExStringMatchMode::StartsWith:
				if (Str.StartsWith(IdentifierStr)) { bMatches = true; }
				break;
			case EPCGExStringMatchMode::EndsWith:
				if (Str.EndsWith(IdentifierStr)) { bMatches = true; }
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
