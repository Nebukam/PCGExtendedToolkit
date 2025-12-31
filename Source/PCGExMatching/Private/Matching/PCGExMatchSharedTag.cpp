// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Matching/PCGExMatchSharedTag.h"

#include "Data/PCGExAttributeBroadcaster.h"
#include "Data/PCGExDataTags.h"
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

bool FPCGExMatchSharedTag::PrepareForMatchableSources(FPCGExContext* InContext, const TSharedPtr<TArray<FPCGExTaggedData>>& InMatchableSources)
{
	if (!FPCGExMatchRuleOperation::PrepareForMatchableSources(InContext, InMatchableSources)) { return false; }

	TArray<FPCGExTaggedData>& MatchableSourcesRef = *InMatchableSources.Get();

	TagNameGetters.Reserve(MatchableSourcesRef.Num());
	Tags.Reserve(MatchableSourcesRef.Num());
	if (Config.TagNameInput == EPCGExInputValueType::Attribute)
	{
		for (const FPCGExTaggedData& TaggedData : MatchableSourcesRef)
		{
			TSharedPtr<PCGExData::TAttributeBroadcaster<FString>> Getter = MakeShared<PCGExData::TAttributeBroadcaster<FString>>();

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

bool FPCGExMatchSharedTag::Test(const PCGExData::FConstPoint& InTargetElement, const FPCGExTaggedData& InCandidate, const PCGExMatching::FScope& InMatchingScope) const
{
	FString TestTagName = TagNameGetters.IsEmpty() ? Config.TagName : TagNameGetters[InTargetElement.IO]->FetchSingle(InTargetElement, TEXT(""));
	bool bDoValueMatch = Config.bDoValueMatch;

	// If the raw string in the tag:value format, enforce value check 
	if (TSharedPtr<PCGExData::IDataValue> Value = PCGExData::TryGetValueFromTag(TestTagName, TestTagName)) { bDoValueMatch = true; }

	TSharedPtr<PCGExData::FTags> TargetTags = Tags[InTargetElement.IO].Pin();
	if (!TargetTags) { return false; }

	TSharedPtr<PCGExData::IDataValue> TargetValue = TargetTags->GetValue(TestTagName);
	TSharedPtr<PCGExData::IDataValue> SourceValue = InCandidate.GetTags()->GetValue(TestTagName);

	if (bDoValueMatch)
	{
		if (!TargetValue || !SourceValue) { return false; }
		return TargetValue->SameValue(SourceValue);
	}

	if (TargetValue && SourceValue) { return true; }
	if (TargetValue || SourceValue) { return false; }

	return TargetTags->RawTags.Contains(TestTagName) && InCandidate.GetTags()->RawTags.Contains(TestTagName);
}

bool UPCGExMatchSharedTagFactory::WantsPoints()
{
	return Config.TagNameInput == EPCGExInputValueType::Attribute && !PCGExMetaHelpers::IsDataDomainAttribute(Config.TagNameAttribute);
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
