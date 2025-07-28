// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Matching/PCGExMatchTagToAttr.h"


#define LOCTEXT_NAMESPACE "PCGExMatchTagToAttr"
#define PCGEX_NAMESPACE MatchTagToAttr

void FPCGExMatchTagToAttrConfig::Init()
{
	FPCGAttributePropertyInputSelector S;
	//S.Update(CandidateAttributeName.ToString());
	//CandidateAttributeName_Sanitized = S.GetAttributeName();

	FPCGExMatchRuleConfigBase::Init();
}

bool FPCGExMatchTagToAttr::PrepareForTargets(FPCGExContext* InContext, const TSharedPtr<TArray<PCGExData::FTaggedData>>& InTargets)
{
	if (!FPCGExMatchRuleOperation::PrepareForTargets(InContext, InTargets)) { return false; }

	TArray<PCGExData::FTaggedData>& TargetsRef = *InTargets.Get();

	TagNameGetters.Reserve(TargetsRef.Num());
	if (Config.TagNameInput == EPCGExInputValueType::Attribute)
	{
		for (const PCGExData::FTaggedData& TaggedData : TargetsRef)
		{
			TSharedPtr<PCGEx::TAttributeBroadcaster<FString>> Getter = MakeShared<PCGEx::TAttributeBroadcaster<FString>>();

			if (!Getter->PrepareForSingleFetch(Config.TagNameAttribute, TaggedData))
			{
				PCGEX_LOG_INVALID_ATTR_C(InContext, Tag Name, Config.TagNameAttribute)
				return false;
			}

			TagNameGetters.Add(Getter);
		}
	}

	if (!Config.bDoValueMatch) { return true; }

	switch (Config.ValueType)
	{
	case EPCGExComparisonDataType::Numeric:
		NumGetters.Reserve(TargetsRef.Num());
		for (const PCGExData::FTaggedData& TaggedData : TargetsRef)
		{
			TSharedPtr<PCGEx::TAttributeBroadcaster<double>> Getter = MakeShared<PCGEx::TAttributeBroadcaster<double>>();

			if (!Getter->PrepareForSingleFetch(Config.ValueAttribute, TaggedData))
			{
				PCGEX_LOG_INVALID_SELECTOR_C(InContext, Value, Config.ValueAttribute)
				return false;
			}

			NumGetters.Add(Getter);
		}
		break;
	case EPCGExComparisonDataType::String:
		StrGetters.Reserve(TargetsRef.Num());
		for (const PCGExData::FTaggedData& TaggedData : TargetsRef)
		{
			TSharedPtr<PCGEx::TAttributeBroadcaster<FString>> Getter = MakeShared<PCGEx::TAttributeBroadcaster<FString>>();

			if (!Getter->PrepareForSingleFetch(Config.ValueAttribute, TaggedData))
			{
				PCGEX_LOG_INVALID_SELECTOR_C(InContext, Value, Config.ValueAttribute)
				return false;
			}

			StrGetters.Add(Getter);
		}
		break;
	}

	return true;
}

bool FPCGExMatchTagToAttr::Test(const PCGExData::FConstPoint& InTargetElement, const TSharedPtr<PCGExData::FPointIO>& PointIO, const PCGExMatching::FMatchingScope& InMatchingScope) const
{
	const FString TestTagName = TagNameGetters.IsEmpty() ? Config.TagName : TagNameGetters[InTargetElement.IO]->FetchSingle(InTargetElement, TEXT(""));

	if (!Config.bDoValueMatch)
	{
		return PCGExCompare::HasMatchingTags(PointIO->Tags, TestTagName, Config.NameMatch);
	}


	TArray<TSharedPtr<PCGExData::IDataValue>> TagValues;
	if (!PCGExCompare::GetMatchingValueTags(PointIO->Tags, TestTagName, Config.NameMatch, TagValues)) { return false; }

	if (Config.ValueType == EPCGExComparisonDataType::Numeric)
	{
		const double OperandBNumeric = NumGetters[InTargetElement.IO]->FetchSingle(InTargetElement, 0);
		for (const TSharedPtr<PCGExData::IDataValue>& TagValue : TagValues)
		{
			if (!PCGExCompare::Compare(Config.NumericComparison, TagValue, OperandBNumeric, Config.Tolerance)) { return false; }
		}
	}
	else
	{
		const FString OperandBString = StrGetters[InTargetElement.IO]->FetchSingle(InTargetElement, TEXT(""));
		for (const TSharedPtr<PCGExData::IDataValue>& TagValue : TagValues)
		{
			if (!PCGExCompare::Compare(Config.StringComparison, TagValue, OperandBString)) { return false; }
		}
	}

	return true;
}

bool UPCGExMatchTagToAttrFactory::WantsPoints()
{
	if (Config.TagNameInput == EPCGExInputValueType::Attribute && !PCGExHelpers::IsDataDomainAttribute(Config.TagNameAttribute)) { return true; }
	if (!Config.bDoValueMatch) { return false; }
	return !PCGExHelpers::IsDataDomainAttribute(Config.ValueAttribute);
}

PCGEX_MATCH_RULE_BOILERPLATE(TagToAttr)

#if WITH_EDITOR
FString UPCGExCreateMatchTagToAttrSettings::GetDisplayName() const
{
	FString TagSourceStr = Config.TagNameInput == EPCGExInputValueType::Constant ? Config.TagName : TEXT("Tag \"") + Config.TagNameAttribute.ToString() + TEXT("\"");

	if (Config.bDoValueMatch)
	{
		TagSourceStr += TEXT("::Value ") + PCGExCompare::ToString(Config.NameMatch);

		if (Config.ValueType == EPCGExComparisonDataType::Numeric) { TagSourceStr += PCGExCompare::ToString(Config.NumericComparison); }
		else { TagSourceStr += PCGExCompare::ToString(Config.StringComparison); }

		TagSourceStr += TEXT("Target' @") + PCGEx::GetSelectorDisplayName(Config.ValueAttribute);
	}
	else
	{
		TagSourceStr += PCGExCompare::ToString(Config.NameMatch) + TEXT("Target' @");
		TagSourceStr += Config.TagNameInput == EPCGExInputValueType::Constant ? Config.TagName : Config.TagNameAttribute.ToString();
	}

	return TagSourceStr;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
