// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Matching/PCGExMatchByIndex.h"

#include "PCGExHelpers.h"
#include "PCGExMath.h"
#include "Data/PCGExAttributeBroadcaster.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExPointIO.h"


#define LOCTEXT_NAMESPACE "PCGExMatchByIndex"
#define PCGEX_NAMESPACE MatchByIndex

void FPCGExMatchByIndexConfig::Init()
{
	FPCGExMatchRuleConfigBase::Init();
}

bool FPCGExMatchByIndex::PrepareForTargets(FPCGExContext* InContext, const TSharedPtr<TArray<FPCGExTaggedData>>& InTargets)
{
	if (!FPCGExMatchRuleOperation::PrepareForTargets(InContext, InTargets)) { return false; }

	TArray<FPCGExTaggedData>& TargetsRef = *InTargets.Get();

	bIsIndex = Config.IndexAttribute.GetSelection() == EPCGAttributePropertySelection::ExtraProperty && Config.IndexAttribute.GetExtraProperty() == EPCGExtraProperties::Index;

	if (!bIsIndex && Config.Source == EPCGExMatchByIndexSource::Target)
	{
		IndexGetters.Reserve(TargetsRef.Num());
		for (const FPCGExTaggedData& TaggedData : TargetsRef)
		{
			TSharedPtr<PCGExData::TAttributeBroadcaster<int32>> Getter = MakeShared<PCGExData::TAttributeBroadcaster<int32>>();

			if (!Getter->PrepareForSingleFetch(Config.IndexAttribute, TaggedData))
			{
				PCGEX_LOG_INVALID_SELECTOR_C(InContext, Index Attribute, Config.IndexAttribute)
				return false;
			}

			IndexGetters.Add(Getter);
		}
	}

	return true;
}

bool FPCGExMatchByIndex::Test(const PCGExData::FConstPoint& InTargetElement, const TSharedPtr<PCGExData::FPointIO>& PointIO, const PCGExMatching::FMatchingScope& InMatchingScope) const
{
	int32 IndexValue = -1;
	int32 OtherIndex = -1;

	if (Config.Source == EPCGExMatchByIndexSource::Target)
	{
		if (bIsIndex) { IndexValue = InTargetElement.Data ? InTargetElement.Index : InTargetElement.IO; }
		else { IndexValue = IndexGetters[InTargetElement.IO]->FetchSingle(InTargetElement, -1); }

		OtherIndex = PointIO->IOIndex;

		IndexValue = PCGExMath::SanitizeIndex(IndexValue, InMatchingScope.GetNumCandidates() - 1, Config.IndexSafety);
	}
	else
	{
		if (!PCGExData::Helpers::TryReadDataValue<int32>(PointIO, Config.IndexAttribute, IndexValue)) { return false; }
		OtherIndex = InTargetElement.Data ? InTargetElement.Index : InTargetElement.IO;

		IndexValue = PCGExMath::SanitizeIndex(IndexValue, InTargetElement.Data ? InTargetElement.Data->GetNumPoints() - 1 : Targets->Num() - 1, Config.IndexSafety);
	}

	if (IndexValue == -1 || OtherIndex == -1) { return false; }
	return IndexValue == OtherIndex;
}

bool UPCGExMatchByIndexFactory::WantsPoints()
{
	return !PCGExMetaHelpers::IsDataDomainAttribute(Config.IndexAttribute);
}

PCGEX_MATCH_RULE_BOILERPLATE(ByIndex)

#if WITH_EDITOR
FString UPCGExCreateMatchByIndexSettings::GetDisplayName() const
{
	if (Config.Source == EPCGExMatchByIndexSource::Target)
	{
		return TEXT("Target' ") + PCGExMetaHelpers::GetSelectorDisplayName(Config.IndexAttribute) + TEXT(" == Input Index");
	}
	return TEXT("Input' ") + PCGExMetaHelpers::GetSelectorDisplayName(Config.IndexAttribute) + TEXT(" == Target Index");
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
