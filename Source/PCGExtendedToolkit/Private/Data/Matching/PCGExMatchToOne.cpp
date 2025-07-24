// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Matching/PCGExMatchToOne.h"




#define LOCTEXT_NAMESPACE "PCGExMatchToOne"
#define PCGEX_NAMESPACE MatchToOne

bool FPCGExMatchToOne::PrepareForTargets(FPCGExContext* InContext, const TSharedPtr<TArray<PCGExData::FTaggedData>>& InTargets)
{
	return FPCGExMatchRuleOperation::PrepareForTargets(InContext, InTargets);
}

bool FPCGExMatchToOne::Test(const PCGExData::FConstPoint& InTargetElement, const TSharedPtr<PCGExData::FPointIO>& PointIO, const PCGExMatching::FMatchingScope& InMatchingScope) const
{
	return PointIO->IOIndex == InTargetElement.IO;
}

PCGEX_MATCH_RULE_BOILERPLATE(ToOne)

#if WITH_EDITOR
FString UPCGExCreateMatchToOneSettings::GetDisplayName() const
{
	return TEXT("Match 1:1");
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
