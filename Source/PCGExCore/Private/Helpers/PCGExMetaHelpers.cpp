// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Helpers/PCGExMetaHelpers.h"
#include "CoreMinimal.h"
#include "PCGExCommon.h"

namespace PCGExMetaHelpers
{
	bool IsPCGExAttribute(const FString& InStr) { return InStr.Contains(PCGExCommon::PCGExPrefix); }

	bool IsPCGExAttribute(const FName InName) { return IsPCGExAttribute(InName.ToString()); }

	bool IsPCGExAttribute(const FText& InText) { return IsPCGExAttribute(InText.ToString()); }

	FName MakePCGExAttributeName(const FString& Str0)
	{
		return FName(FText::Format(FText::FromString(TEXT("{0}{1}")), FText::FromString(PCGExCommon::PCGExPrefix), FText::FromString(Str0)).ToString());
	}

	FName MakePCGExAttributeName(const FString& Str0, const FString& Str1)
	{
		return FName(FText::Format(FText::FromString(TEXT("{0}{1}/{2}")), FText::FromString(PCGExCommon::PCGExPrefix), FText::FromString(Str0), FText::FromString(Str1)).ToString());
	}

	bool IsWritableAttributeName(const FName Name)
	{
		// This is a very expensive check, however it's also futureproofing 
		if (Name.IsNone()) { return false; }

		FPCGAttributePropertyInputSelector DummySelector;
		if (!DummySelector.Update(Name.ToString())) { return false; }
		return DummySelector.GetSelection() == EPCGAttributePropertySelection::Attribute && DummySelector.IsValid();
	}

	FString StringTagFromName(const FName Name)
	{
		if (Name.IsNone()) { return TEXT(""); }
		return Name.ToString().TrimStartAndEnd();
	}

	bool IsValidStringTag(const FString& Tag)
	{
		if (Tag.TrimStartAndEnd().IsEmpty()) { return false; }
		return true;
	}

	FName GetCompoundName(const FName A, const FName B)
	{
		// PCGEx/A/B
		const FString Separator = TEXT("/");
		return *(TEXT("PCGEx") + Separator + A.ToString() + Separator + B.ToString());
	}

	FName GetCompoundName(const FName A, const FName B, const FName C)
	{
		// PCGEx/A/B/C
		const FString Separator = TEXT("/");
		return *(TEXT("PCGEx") + Separator + A.ToString() + Separator + B.ToString() + Separator + C.ToString());
	}

	bool TryGetAttributeName(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData, FName& OutName)
	{
		FPCGAttributePropertyInputSelector FixedSelector = InSelector.CopyAndFixLast(InData);
		if (!FixedSelector.IsValid()) { return false; }
		if (FixedSelector.GetSelection() != EPCGAttributePropertySelection::Attribute) { return false; }
		OutName = FixedSelector.GetName();
		return true;
	}

	bool IsDataDomainAttribute(const FName& InName)
	{
		return InName.ToString().TrimStartAndEnd().StartsWith("@Data.");
	}

	bool IsDataDomainAttribute(const FString& InName)
	{
		return InName.TrimStartAndEnd().StartsWith("@Data.");
	}

	bool IsDataDomainAttribute(const FPCGAttributePropertyInputSelector& InputSelector)
	{
		return InputSelector.GetDomainName() == PCGDataConstants::DataDomainName || IsDataDomainAttribute(InputSelector.GetName());
	}

	void AppendUniqueSelectorsFromCommaSeparatedList(const FString& InCommaSeparatedString, TArray<FPCGAttributePropertyInputSelector>& OutSelectors)
	{
		if (InCommaSeparatedString.IsEmpty()) { return; }

		TArray<FString> Result;
		InCommaSeparatedString.ParseIntoArray(Result, TEXT(","));
		// Trim leading and trailing spaces
		for (int i = 0; i < Result.Num(); i++)
		{
			FString& String = Result[i];
			String.TrimStartAndEndInline();
			if (String.IsEmpty()) { continue; }

			FPCGAttributePropertyInputSelector Selector;
			Selector.Update(String);

			OutSelectors.AddUnique(Selector);
		}
	}

	FName GetLongNameFromSelector(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData, const bool bInitialized)
	{
		// This return a domain-less unique identifier for the provided selector
		// It's mostly used to create uniquely identified value buffers

		if (!InData) { return InvalidName; }

		if (bInitialized)
		{
			if (InSelector.GetExtraNames().IsEmpty()) { return FName(InSelector.GetName().ToString()); }
			return FName(InSelector.GetName().ToString() + TEXT(".") + FString::Join(InSelector.GetExtraNames(), TEXT(".")));
		}
		if (InSelector.GetSelection() == EPCGAttributePropertySelection::Attribute && InSelector.GetName() == "@Last")
		{
			return GetLongNameFromSelector(InSelector.CopyAndFixLast(InData), InData, true);
		}

		return GetLongNameFromSelector(InSelector, InData, true);
	}

	FPCGAttributeIdentifier GetAttributeIdentifier(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData, const bool bInitialized)
	{
		// This return an identifier suitable to be used for data facade

		FPCGAttributeIdentifier Identifier;

		if (!InData) { return FPCGAttributeIdentifier(InvalidName, EPCGMetadataDomainFlag::Invalid); }

		if (bInitialized)
		{
			Identifier.Name = InSelector.GetAttributeName();
			Identifier.MetadataDomain = InData->GetMetadataDomainIDFromSelector(InSelector);
		}
		else
		{
			FPCGAttributePropertyInputSelector FixedSelector = InSelector.CopyAndFixLast(InData);

			check(FixedSelector.GetSelection() == EPCGAttributePropertySelection::Attribute)

			Identifier.Name = FixedSelector.GetAttributeName();
			Identifier.MetadataDomain = InData->GetMetadataDomainIDFromSelector(FixedSelector);
		}


		return Identifier;
	}

	FPCGAttributeIdentifier GetAttributeIdentifier(const FName InName, const UPCGData* InData)
	{
		FPCGAttributePropertyInputSelector Selector;
		Selector.Update(InName.ToString());
		Selector = Selector.CopyAndFixLast(InData);
		return GetAttributeIdentifier(Selector, InData);
	}

	FPCGAttributeIdentifier GetAttributeIdentifier(const FName InName)
	{
		FString StrName = InName.ToString();
		FPCGAttributePropertyInputSelector Selector;
		Selector.Update(InName.ToString());
		return FPCGAttributeIdentifier(Selector.GetAttributeName(), StrName.StartsWith(TEXT("@Data.")) ? PCGMetadataDomainID::Data : PCGMetadataDomainID::Elements);
	}

	FPCGAttributePropertyInputSelector GetSelectorFromIdentifier(const FPCGAttributeIdentifier& InIdentifier)
	{
		FPCGAttributePropertyInputSelector Selector;
		Selector.SetAttributeName(InIdentifier.Name);
		Selector.SetDomainName(InIdentifier.MetadataDomain.DebugName);
		return Selector;
	}

	bool HasAttribute(const UPCGMetadata* InMetadata, const FPCGAttributeIdentifier& Identifier)
	{
		if (!InMetadata) { return false; }
		if (!InMetadata->GetConstMetadataDomain(Identifier.MetadataDomain)) { return false; }
		return InMetadata->HasAttribute(Identifier);
	}

	FString GetSelectorDisplayName(const FPCGAttributePropertyInputSelector& InSelector)
	{
		if (InSelector.GetExtraNames().IsEmpty()) { return InSelector.GetName().ToString(); }
		return InSelector.GetName().ToString() + TEXT(".") + FString::Join(InSelector.GetExtraNames(), TEXT("."));
	}
}
