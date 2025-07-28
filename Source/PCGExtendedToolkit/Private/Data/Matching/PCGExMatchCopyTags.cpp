// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Matching/PCGExMatchCopyTags.h"


#define LOCTEXT_NAMESPACE "PCGExMatchCopyTags"
#define PCGEX_NAMESPACE MatchCopyTags

bool FPCGExMatchCopyTags::PrepareForTargets(FPCGExContext* InContext, const TSharedPtr<TArray<PCGExData::FTaggedData>>& InTargets)
{
	return FPCGExMatchRuleOperation::PrepareForTargets(InContext, InTargets);
}

bool FPCGExMatchCopyTags::Test(const PCGExData::FConstPoint& InTargetElement, const TSharedPtr<PCGExData::FPointIO>& PointIO, const PCGExMatching::FMatchingScope& InMatchingScope) const
{
	if (InTargetElement.Data) { return true; }

	PCGExData::FTaggedData* TaggedData = Targets->GetData() + InTargetElement.IO;
	if (const TSharedPtr<PCGExData::FTags> Tags = TaggedData->GetTags()) { PointIO->Tags->Append(Tags.ToSharedRef()); }
	return true;
}

PCGEX_MATCH_RULE_BOILERPLATE(CopyTags)

#if WITH_EDITOR
FString UPCGExCreateMatchCopyTagsSettings::GetDisplayName() const
{
	return TEXT("Match Copy Tags");
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
