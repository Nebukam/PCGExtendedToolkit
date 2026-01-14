// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Matching/PCGExMatchOverlap.h"

#include "Data/PCGExAttributeBroadcaster.h"
#include "Data/PCGExPointIO.h"
#include "Factories/PCGExFactoryData.h"


#define LOCTEXT_NAMESPACE "PCGExMatchOverlap"
#define PCGEX_NAMESPACE MatchOverlap


bool FPCGExMatchOverlap::PrepareForMatchableSources(FPCGExContext* InContext, const TSharedPtr<TArray<FPCGExTaggedData>>& InMatchableSources)
{
	if (!FPCGExMatchRuleOperation::PrepareForMatchableSources(InContext, InMatchableSources)) { return false; }

	// TODO

	return true;
}

bool FPCGExMatchOverlap::Test(const PCGExData::FConstPoint& InTargetElement, const FPCGExTaggedData& InCandidate, const PCGExMatching::FScope& InMatchingScope) const
{
	// TODO

	return true;
}

bool UPCGExMatchOverlapFactory::WantsPoints()
{
	return !PCGExMetaHelpers::IsDataDomainAttribute(Config.Expansion.Attribute);
}

PCGEX_MATCH_RULE_BOILERPLATE(Overlap)

#if WITH_EDITOR
FString UPCGExCreateMatchOverlapSettings::GetDisplayName() const
{
	// TODO

	return TEXT("");
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
