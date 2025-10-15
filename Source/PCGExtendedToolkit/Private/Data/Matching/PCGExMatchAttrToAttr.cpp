// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Matching/PCGExMatchAttrToAttr.h"

#include "PCGExHelpers.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExPointElements.h"


#define LOCTEXT_NAMESPACE "PCGExMatchAttrToAttr"
#define PCGEX_NAMESPACE MatchAttrToAttr

void FPCGExMatchAttrToAttrConfig::Init()
{
	FPCGAttributePropertyInputSelector S;
	S.Update(CandidateAttributeName.ToString());
	CandidateAttributeName_Sanitized = S.GetAttributeName();

	FPCGExMatchRuleConfigBase::Init();
}

bool FPCGExMatchAttrToAttr::PrepareForTargets(FPCGExContext* InContext, const TSharedPtr<TArray<PCGExData::FTaggedData>>& InTargets)
{
	if (!FPCGExMatchRuleOperation::PrepareForTargets(InContext, InTargets)) { return false; }

	TArray<PCGExData::FTaggedData>& TargetsRef = *InTargets.Get();

	switch (Config.Check)
	{
	case EPCGExComparisonDataType::Numeric:
		NumGetters.Reserve(TargetsRef.Num());
		for (const PCGExData::FTaggedData& TaggedData : TargetsRef)
		{
			TSharedPtr<PCGEx::TAttributeBroadcaster<double>> Getter = MakeShared<PCGEx::TAttributeBroadcaster<double>>();

			if (!Getter->PrepareForSingleFetch(Config.TargetAttributeName, TaggedData))
			{
				PCGEX_LOG_INVALID_ATTR_C(InContext, Target Attribute, Config.TargetAttributeName)
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

			if (!Getter->PrepareForSingleFetch(Config.TargetAttributeName, TaggedData))
			{
				PCGEX_LOG_INVALID_ATTR_C(InContext, Target Attribute, Config.TargetAttributeName)
				return false;
			}

			StrGetters.Add(Getter);
		}
		break;
	}

	return true;
}

bool FPCGExMatchAttrToAttr::Test(const PCGExData::FConstPoint& InTargetElement, const TSharedPtr<PCGExData::FPointIO>& PointIO, const PCGExMatching::FMatchingScope& InMatchingScope) const
{
	if (Config.Check == EPCGExComparisonDataType::Numeric)
	{
		const double TargetValue = NumGetters[InTargetElement.IO]->FetchSingle(InTargetElement, MAX_dbl);
		double CandidateValue = 0;

		if (!PCGExDataHelpers::TryReadDataValue<double>(PointIO, Config.CandidateAttributeName_Sanitized, CandidateValue)) { return false; }

		return Config.bSwapOperands ?
			       PCGExCompare::Compare(Config.NumericComparison, TargetValue, CandidateValue, Config.Tolerance) :
			       PCGExCompare::Compare(Config.NumericComparison, CandidateValue, TargetValue, Config.Tolerance);
	}
	const FString TargetValue = StrGetters[InTargetElement.IO]->FetchSingle(InTargetElement, TEXT(""));
	FString CandidateValue = TEXT("");

	if (!PCGExDataHelpers::TryReadDataValue<FString>(PointIO, Config.CandidateAttributeName_Sanitized, CandidateValue)) { return false; }

	return Config.bSwapOperands ?
		       PCGExCompare::Compare(Config.StringComparison, TargetValue, CandidateValue) :
		       PCGExCompare::Compare(Config.StringComparison, CandidateValue, TargetValue);
}

bool UPCGExMatchAttrToAttrFactory::WantsPoints()
{
	return !PCGExHelpers::IsDataDomainAttribute(Config.TargetAttributeName);
}

PCGEX_MATCH_RULE_BOILERPLATE(AttrToAttr)

#if WITH_EDITOR
FString UPCGExCreateMatchAttrToAttrSettings::GetDisplayName() const
{
	if (Config.Check == EPCGExComparisonDataType::Numeric)
	{
		return Config.bSwapOperands ?
			       Config.TargetAttributeName.ToString() + PCGExCompare::ToString(Config.NumericComparison) + Config.CandidateAttributeName.ToString() :
			       Config.CandidateAttributeName.ToString() + PCGExCompare::ToString(Config.NumericComparison) + Config.TargetAttributeName.ToString();
	}
	return Config.bSwapOperands ?
		       Config.TargetAttributeName.ToString() + PCGExCompare::ToString(Config.StringComparison) + Config.CandidateAttributeName.ToString() :
		       Config.CandidateAttributeName.ToString() + PCGExCompare::ToString(Config.StringComparison) + Config.TargetAttributeName.ToString();
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
