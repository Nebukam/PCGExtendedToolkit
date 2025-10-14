// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Matching/PCGExMatchSharedTag.h"

#include "PCGExHelpers.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointIO.h"


#define LOCTEXT_NAMESPACE "PCGExMatchSharedTag"
#define PCGEX_NAMESPACE MatchSharedTag

void FPCGExMatchSharedTagConfig::Init()
{
	FPCGAttributePropertyInputSelector S;
	//S.Update(CandidateAttributeName.ToString());
	//CandidateAttributeName_Sanitized = S.GetAttributeName();

	FPCGExMatchRuleConfigBase::Init();
}

bool FPCGExMatchSharedTag::PrepareForTargets(FPCGExContext* InContext, const TSharedPtr<TArray<PCGExData::FTaggedData>>& InTargets)
{
	if (!FPCGExMatchRuleOperation::PrepareForTargets(InContext, InTargets)) { return false; }

	TArray<PCGExData::FTaggedData>& TargetsRef = *InTargets.Get();

	TagNameGetters.Reserve(TargetsRef.Num());
	Tags.Reserve(TargetsRef.Num());
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
			Tags.Add(TaggedData.Tags);
		}
	}

	return true;
}

bool FPCGExMatchSharedTag::Test(const PCGExData::FConstPoint& InTargetElement, const TSharedPtr<PCGExData::FPointIO>& PointIO, const PCGExMatching::FMatchingScope& InMatchingScope) const
{
	FString TestTagName = TagNameGetters.IsEmpty() ? Config.TagName : TagNameGetters[InTargetElement.IO]->FetchSingle(InTargetElement, TEXT(""));
	bool bDoValueMatch = Config.bDoValueMatch;

	// If the raw string in the tag:value format, enforce value check 
	if (TSharedPtr<PCGExData::IDataValue> Value = PCGExData::TryGetValueFromTag(TestTagName, TestTagName)) { bDoValueMatch = true; }

	TSharedPtr<PCGExData::FTags> TargetTags = Tags[InTargetElement.IO].Pin();
	if (!TargetTags) { return false; }

	TSharedPtr<PCGExData::IDataValue> TargetValue = TargetTags->GetValue(TestTagName);
	TSharedPtr<PCGExData::IDataValue> SourceValue = PointIO->Tags->GetValue(TestTagName);

	if (bDoValueMatch)
	{
		if (!TargetValue || !SourceValue) { return false; }
		return TargetValue->SameValue(SourceValue);
	}

	if (TargetValue && SourceValue) { return true; }
	if (TargetValue || SourceValue) { return false; }

	return TargetTags->RawTags.Contains(TestTagName) && PointIO->Tags->RawTags.Contains(TestTagName);
}

bool UPCGExMatchSharedTagFactory::WantsPoints()
{
	return Config.TagNameInput == EPCGExInputValueType::Attribute && !PCGExHelpers::IsDataDomainAttribute(Config.TagNameAttribute);
}

PCGEX_MATCH_RULE_BOILERPLATE(SharedTag)

#if WITH_EDITOR
FString UPCGExCreateMatchSharedTagSettings::GetDisplayName() const
{
	FString NameStr = TEXT("Share ");
	NameStr += Config.TagNameInput == EPCGExInputValueType::Constant ? Config.TagName : TEXT("Tag \"") + Config.TagNameAttribute.ToString() + TEXT("\"");

	return NameStr;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
