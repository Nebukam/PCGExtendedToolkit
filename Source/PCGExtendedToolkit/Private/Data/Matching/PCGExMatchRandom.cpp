// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Matching/PCGExMatchRandom.h"

#include "PCGExHelpers.h"
#include "PCGExRandom.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExPointIO.h"


#define LOCTEXT_NAMESPACE "PCGExMatchRandom"
#define PCGEX_NAMESPACE MatchRandom

FPCGExMatchRandomConfig::FPCGExMatchRandomConfig()
	: FPCGExMatchRuleConfigBase()
{
	ThresholdAttribute.Update("@Data.Threshold");
}

bool FPCGExMatchRandom::PrepareForTargets(FPCGExContext* InContext, const TSharedPtr<TArray<PCGExData::FTaggedData>>& InTargets)
{
	if (!FPCGExMatchRuleOperation::PrepareForTargets(InContext, InTargets)) { return false; }

	TArray<PCGExData::FTaggedData>& TargetsRef = *InTargets.Get();

	if (Config.ThresholdInput == EPCGExInputValueType::Attribute)
	{
		ThresholdGetters.Reserve(TargetsRef.Num());
		for (const PCGExData::FTaggedData& TaggedData : TargetsRef)
		{
			TSharedPtr<PCGEx::TAttributeBroadcaster<double>> Getter = MakeShared<PCGEx::TAttributeBroadcaster<double>>();

			if (!Getter->PrepareForSingleFetch(Config.ThresholdAttribute, TaggedData))
			{
				PCGEX_LOG_INVALID_SELECTOR_C(InContext, Index Attribute, Config.ThresholdAttribute)
				return false;
			}

			ThresholdGetters.Add(Getter);
		}
	}

	return true;
}

bool FPCGExMatchRandom::Test(const PCGExData::FConstPoint& InTargetElement, const TSharedPtr<PCGExData::FPointIO>& PointIO, const PCGExMatching::FMatchingScope& InMatchingScope) const
{
	const double LocalThreshold = ThresholdGetters.IsEmpty() ? Config.Threshold : ThresholdGetters[InTargetElement.IO]->FetchSingle(InTargetElement, Config.Threshold);
	const float RandomValue = FRandomStream(PCGExRandom::GetRandomStreamFromPoint(Config.RandomSeed + InTargetElement.IO, PointIO->IOIndex)).GetFraction();
	return Config.bInvertThreshold ? RandomValue <= LocalThreshold : RandomValue >= LocalThreshold;
}

bool UPCGExMatchRandomFactory::WantsPoints()
{
	return !PCGExHelpers::IsDataDomainAttribute(Config.ThresholdAttribute);
}

PCGEX_MATCH_RULE_BOILERPLATE(Random)

#if WITH_EDITOR
FString UPCGExCreateMatchRandomSettings::GetDisplayName() const
{
	return TEXT("Random");
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
