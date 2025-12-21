// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Matching/PCGExMatchRandom.h"

#include "Data/PCGExAttributeBroadcaster.h"
#include "Data/PCGExPointIO.h"
#include "Helpers/PCGExRandomHelpers.h"


#define LOCTEXT_NAMESPACE "PCGExMatchRandom"
#define PCGEX_NAMESPACE MatchRandom

FPCGExMatchRandomConfig::FPCGExMatchRandomConfig()
	: FPCGExMatchRuleConfigBase()
{
	ThresholdAttribute.Update("@Data.Threshold");
}

bool FPCGExMatchRandom::PrepareForTargets(FPCGExContext* InContext, const TSharedPtr<TArray<FPCGExTaggedData>>& InTargets)
{
	if (!FPCGExMatchRuleOperation::PrepareForTargets(InContext, InTargets)) { return false; }

	TArray<FPCGExTaggedData>& TargetsRef = *InTargets.Get();

	if (Config.ThresholdInput == EPCGExInputValueType::Attribute)
	{
		ThresholdGetters.Reserve(TargetsRef.Num());
		for (const FPCGExTaggedData& TaggedData : TargetsRef)
		{
			TSharedPtr<PCGExData::TAttributeBroadcaster<double>> Getter = MakeShared<PCGExData::TAttributeBroadcaster<double>>();

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

bool FPCGExMatchRandom::Test(const PCGExData::FConstPoint& InTargetElement, const TSharedPtr<PCGExData::FPointIO>& PointIO, const PCGExMatching::FScope& InMatchingScope) const
{
	const double LocalThreshold = ThresholdGetters.IsEmpty() ? Config.Threshold : ThresholdGetters[InTargetElement.IO]->FetchSingle(InTargetElement, Config.Threshold);
	const float RandomValue = FRandomStream(PCGExRandomHelpers::GetRandomStreamFromPoint(Config.RandomSeed + InTargetElement.IO, PointIO->IOIndex)).GetFraction();
	return Config.bInvertThreshold ? RandomValue <= LocalThreshold : RandomValue >= LocalThreshold;
}

bool UPCGExMatchRandomFactory::WantsPoints()
{
	return !PCGExMetaHelpers::IsDataDomainAttribute(Config.ThresholdAttribute);
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
