// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Matching/PCGExMatchByIndex.h"


#define LOCTEXT_NAMESPACE "PCGExMatchByIndex"
#define PCGEX_NAMESPACE MatchByIndex

void FPCGExMatchByIndexConfig::Init()
{
	FPCGExMatchRuleConfigBase::Init();
}

bool FPCGExMatchByIndex::PrepareForTargets(FPCGExContext* InContext, const TSharedPtr<TArray<PCGExData::FTaggedData>>& InTargets)
{
	if (!FPCGExMatchRuleOperation::PrepareForTargets(InContext, InTargets)) { return false; }

	TArray<PCGExData::FTaggedData>& TargetsRef = *InTargets.Get();

	bIsIndex = Config.IndexAttribute.GetSelection() == EPCGAttributePropertySelection::ExtraProperty && Config.IndexAttribute.GetExtraProperty() == EPCGExtraProperties::Index;

	if (Config.Source == EPCGExMatchByIndexSource::Target)
	{
		IndexGetters.Reserve(TargetsRef.Num());
		for (const PCGExData::FTaggedData& TaggedData : TargetsRef)
		{
			TSharedPtr<PCGEx::TAttributeBroadcaster<int32>> Getter = MakeShared<PCGEx::TAttributeBroadcaster<int32>>();

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

bool FPCGExMatchByIndex::Test(const PCGExData::FConstPoint& InTargetElement, const TSharedPtr<PCGExData::FPointIO>& PointIO) const
{
	int32 IndexValue = -1;
	int32 OtherIndex = -1;

	if (Config.Source == EPCGExMatchByIndexSource::Target)
	{
		if (bIsIndex) { IndexValue = InTargetElement.Data ? InTargetElement.Index : InTargetElement.IO; }
		else { IndexValue = IndexGetters[InTargetElement.IO]->FetchSingle(InTargetElement, -1); }
		OtherIndex = PointIO->IOIndex;
	}
	else
	{
		if (!PCGExDataHelpers::TryReadDataValue<int32>(PointIO, Config.IndexAttribute, IndexValue, true)) { return false; }
		OtherIndex = InTargetElement.Data ? InTargetElement.Index : InTargetElement.IO;
	}

	if (IndexValue == -1 || OtherIndex == -1) { return false; }
	return IndexValue == OtherIndex;
}

bool UPCGExMatchByIndexFactory::WantsPoints()
{
	return !PCGExHelpers::IsDataDomainAttribute(Config.IndexAttribute);
}

PCGEX_MATCH_RULE_BOILERPLATE(ByIndex)

#if WITH_EDITOR
FString UPCGExCreateMatchByIndexSettings::GetDisplayName() const
{
	if (Config.Source == EPCGExMatchByIndexSource::Target)
	{
		return TEXT("Target' ") + PCGEx::GetSelectorDisplayName(Config.IndexAttribute) + TEXT(" == Input Index");
	}
	else
	{
		return TEXT("Input' ") + PCGEx::GetSelectorDisplayName(Config.IndexAttribute) + TEXT(" == Target Index");
	}
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
