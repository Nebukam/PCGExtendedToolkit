// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Matching/PCGExMatchAttrToAttrNum.h"

#define LOCTEXT_NAMESPACE "PCGExMatchAttrToAttrNum"
#define PCGEX_NAMESPACE MatchAttrToAttrNum

bool FPCGExMatchAttrToAttrNum::PrepareForTargets(FPCGExContext* InContext, const TSharedPtr<TArray<PCGExData::FTaggedData>>& InTargets)
{
	if (!FPCGExMatchRuleOperation::PrepareForTargets(InContext, InTargets)) { return false; }

	TArray<PCGExData::FTaggedData>& TargetsRef = *InTargets.Get();
	TargetGetters.Reserve(TargetsRef.Num());

	for (const PCGExData::FTaggedData& TaggedData : TargetsRef)
	{
		TSharedPtr<PCGEx::TAttributeBroadcaster<double>> Getter = MakeShared<PCGEx::TAttributeBroadcaster<double>>();
		if (!Getter->PrepareForSingleFetch(Config.TargetAttributeName, TaggedData))
		{
			// TOOD : Log error
			return false;
		}
		TargetGetters.Add(Getter);
	}

	return true;
}

bool FPCGExMatchAttrToAttrNum::Test(const PCGExData::FElement& InTargetElement, const TSharedPtr<PCGExData::FPointIO>& PointIO) const
{
	const double TargetValue = TargetGetters[InTargetElement.IO]->FetchSingle(InTargetElement, MAX_dbl);
	double CandidateValue = 0;
	if (!PCGExDataHelpers::TryReadDataValue<double>(PointIO, Config.CandidateAttributeName, CandidateValue, true)) { return false; }
	return PCGExCompare::Compare(Config.Comparison, CandidateValue, TargetValue);
}

PCGEX_MATCH_RULE_BOILERPLATE(AttrToAttrNum)

#if WITH_EDITOR
FString UPCGExCreateMatchAttrToAttrNumSettings::GetDisplayName() const
{
	return Config.TargetAttributeName.ToString() + TEXT(" ") + Config.CandidateAttributeName.ToString();
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
