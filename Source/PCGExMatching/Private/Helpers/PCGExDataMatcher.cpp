// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Helpers/PCGExDataMatcher.h"

#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExPointIO.h"
#include "Helpers/PCGExArrayHelpers.h"
#include "Core/PCGExMatchRuleFactoryProvider.h"
#include "Details/PCGExMatchingDetails.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"
#include "Metadata/Accessors/PCGCustomAccessor.h"

#define LOCTEXT_NAMESPACE "PCGExCreateMatchRule"
#define PCGEX_NAMESPACE CreateMatchRule

PCGExMatching::FScope::FScope(const int32 InNumCandidates, const bool bUnlimited)
{
	NumCandidates = InNumCandidates;
	if (bUnlimited) { Counter = -MAX_int32; }
}

void PCGExMatching::FScope::RegisterMatch()
{
	FPlatformAtomics::InterlockedIncrement(&Counter);
}

void PCGExMatching::FScope::Invalidate()
{
	FPlatformAtomics::InterlockedExchange(&Valid, false);
}

namespace PCGExMatching
{
	FDataMatcher::FDataMatcher()
	{
		MatchableSources = MakeShared<TArray<FPCGExTaggedData>>();
		MatchableSourceFirstElements = MakeShared<TArray<PCGExData::FConstPoint>>();
	}

	bool FDataMatcher::FindIndex(const UPCGData* InData, int32& OutIndex) const
	{
		const int32* DataIndexPtr = MatchableSourcesMap.Find(InData);
		if (!DataIndexPtr)
		{
			OutIndex = -1;
			return false;
		}

		OutIndex = *DataIndexPtr;
		return true;
	}

	void FDataMatcher::SetDetails(const FPCGExMatchingDetails* InDetails)
	{
		Details = InDetails;
		MatchMode = Details->Mode;
	}

	bool FDataMatcher::Init(FPCGExContext* InContext, const TArray<const UPCGData*>& InMatchableSources, const TArray<TSharedPtr<PCGExData::FTags>>& InTags, const bool bThrowError)
	{
		check(Details)

		check(InMatchableSources.Num() == InTags.Num());

		MatchableSources->Reserve(InMatchableSources.Num());
		for (int i = 0; i < InMatchableSources.Num(); i++) { RegisterTaggedData(InContext, FPCGExTaggedData(InMatchableSources[i], i, InTags[i], nullptr)); }
		return InitInternal(InContext, Labels::SourceMatchRulesLabel);
	}

	bool FDataMatcher::Init(FPCGExContext* InContext, const TArray<TSharedRef<PCGExData::FFacade>>& InMatchableSources, const bool bThrowError)
	{
		check(Details)

		MatchableSources->Reserve(InMatchableSources.Num());

		for (int i = 0; i < InMatchableSources.Num(); i++) { RegisterTaggedData(InContext, InMatchableSources[i]->Source->GetTaggedData(PCGExData::EIOSide::In, i)); }
		return InitInternal(InContext, Labels::SourceMatchRulesLabel);
	}

	bool FDataMatcher::Init(FPCGExContext* InContext, const TArray<TSharedPtr<PCGExData::FFacade>>& InMatchableSources, const bool bThrowError)
	{
		MatchableSources->Reserve(InMatchableSources.Num());

		for (int i = 0; i < InMatchableSources.Num(); i++) { RegisterTaggedData(InContext, InMatchableSources[i]->Source->GetTaggedData(PCGExData::EIOSide::In, i)); }
		return InitInternal(InContext, Labels::SourceMatchRulesLabel);
	}

	bool FDataMatcher::Init(FPCGExContext* InContext, const TArray<FPCGExTaggedData>& InMatchableSources, const bool bThrowError)
	{
		MatchableSources->Reserve(InMatchableSources.Num());
		for (const FPCGExTaggedData& TaggedData : InMatchableSources) { RegisterTaggedData(InContext, TaggedData); }
		return InitInternal(InContext, Labels::SourceMatchRulesLabel);
	}

	bool FDataMatcher::Init(FPCGExContext* InContext, const TSharedPtr<FDataMatcher>& InOtherDataMatcher, const FName InFactoriesLabel, const bool bThrowError)
	{
		MatchableSources = InOtherDataMatcher->MatchableSources;
		MatchableSourceFirstElements = InOtherDataMatcher->MatchableSourceFirstElements;
		MatchableSourcesMap = InOtherDataMatcher->MatchableSourcesMap;

		SetDetails(InOtherDataMatcher->Details);

		return InitInternal(InContext, InFactoriesLabel);
	}

	bool FDataMatcher::Test(const UPCGData* InMatchableSource, const FPCGExTaggedData& InDataCandidate, FScope& InMatchingScope) const
	{
		if (MatchMode == EPCGExMapMatchMode::Disabled || Operations.IsEmpty()) { return true; }

		if (Details->bLimitMatches) { if (!InMatchingScope.IsValid()) { return false; } }

		const int32* DataIndexPtr = MatchableSourcesMap.Find(InMatchableSource);
		if (!DataIndexPtr) { return false; }

		const int32 DataIndex = *DataIndexPtr;
		const PCGExData::FConstPoint& TargetElement = *(MatchableSourceFirstElements->GetData() + DataIndex);

		bool bMatch = true;

		if (MatchMode == EPCGExMapMatchMode::All)
		{
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : RequiredOperations)
			{
				if (!Op->Test(TargetElement, InDataCandidate, InMatchingScope))
				{
					bMatch = false;
					break;
				}
			}

			if (bMatch)
			{
				for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : OptionalOperations)
				{
					if (!Op->Test(TargetElement, InDataCandidate, InMatchingScope))
					{
						bMatch = false;
						break;
					}
				}
			}
		}
		else
		{
			bMatch = false;
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : OptionalOperations)
			{
				if (Op->Test(TargetElement, InDataCandidate, InMatchingScope)) { bMatch = true; }
			}

			if (bMatch)
			{
				for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : RequiredOperations)
				{
					if (!Op->Test(TargetElement, InDataCandidate, InMatchingScope))
					{
						bMatch = false;
						break;
					}
				}
			}
		}

		if (bMatch)
		{
			InMatchingScope.RegisterMatch();
			if (InMatchingScope.GetCounter() > GetMatchLimitFor(InDataCandidate)) { InMatchingScope.Invalidate(); }
		}

		return bMatch;
	}

	bool FDataMatcher::Test(
		const PCGExData::FConstPoint& InInMatchableElement,
		const FPCGExTaggedData& InDataCandidate,
		FScope& InMatchingScope) const
	{
		if (MatchMode == EPCGExMapMatchMode::Disabled || Operations.IsEmpty()) { return true; }

		if (Details->bLimitMatches) { if (!InMatchingScope.IsValid()) { return false; } }

		bool bMatch = true;

		if (MatchMode == EPCGExMapMatchMode::All)
		{
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : RequiredOperations)
			{
				if (!Op->Test(InInMatchableElement, InDataCandidate, InMatchingScope))
				{
					bMatch = false;
					break;
				}
			}

			if (bMatch)
			{
				for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : OptionalOperations)
				{
					if (!Op->Test(InInMatchableElement, InDataCandidate, InMatchingScope))
					{
						bMatch = false;
						break;
					}
				}
			}
		}
		else
		{
			bMatch = false;
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : OptionalOperations)
			{
				if (Op->Test(InInMatchableElement, InDataCandidate, InMatchingScope)) { bMatch = true; }
			}

			if (bMatch)
			{
				for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : RequiredOperations)
				{
					if (!Op->Test(InInMatchableElement, InDataCandidate, InMatchingScope))
					{
						bMatch = false;
						break;
					}
				}
			}
		}

		if (bMatch)
		{
			InMatchingScope.RegisterMatch();
			if (InMatchingScope.GetCounter() >= GetMatchLimitFor(InDataCandidate)) { InMatchingScope.Invalidate(); }
		}

		return bMatch;
	}

	bool FDataMatcher::PopulateIgnoreList(
		const FPCGExTaggedData& InDataCandidate,
		FScope& InMatchingScope, TSet<const UPCGData*>& OutIgnoreList) const
	{
		if (MatchMode == EPCGExMapMatchMode::Disabled) { return true; }

		int32 NumIgnored = 0;
		TArray<FPCGExTaggedData>& MatchableSourcesRef = *MatchableSources.Get();
		for (const FPCGExTaggedData& Source : MatchableSourcesRef)
		{
			if (!Test(Source.Data, InDataCandidate, InMatchingScope))
			{
				OutIgnoreList.Add(Source.Data);
				NumIgnored++;
			}
		}

		return MatchableSources->Num() != NumIgnored;
	}
	int32 FDataMatcher::GetMatchingSourcesIndices(const FPCGExTaggedData& InDataCandidate, FScope& InMatchingScope, TArray<int32>& OutMatches, const TSet<int32>* InExcludedSources) const
	{
		TArray<FPCGExTaggedData>& MatchableSourcesRef = *MatchableSources.Get();
		OutMatches.Reset(NumSources);

		if (InExcludedSources)
		{
			if (MatchMode == EPCGExMapMatchMode::Disabled)
			{
				for (int i = 0; i < NumSources; i++)
				{
					if (InExcludedSources->Contains(i)) { continue; }
					OutMatches.Add(i);
				}

				return OutMatches.Num();
			}

			for (int i = 0; i < MatchableSourcesRef.Num(); i++)
			{
				if (InExcludedSources->Contains(i)) { continue; }
				if (Test(MatchableSourcesRef[i].Data, InDataCandidate, InMatchingScope)) { OutMatches.Add(i); }
			}

			return OutMatches.Num();
		}

		if (MatchMode == EPCGExMapMatchMode::Disabled)
		{
			PCGExArrayHelpers::ArrayOfIndices(OutMatches, MatchableSourcesRef.Num());
			return OutMatches.Num();
		}

		for (int i = 0; i < MatchableSourcesRef.Num(); i++)
		{
			if (Test(MatchableSourcesRef[i].Data, InDataCandidate, InMatchingScope)) { OutMatches.Add(i); }
		}

		return OutMatches.Num();
	}

	bool FDataMatcher::HandleUnmatchedOutput(const TSharedPtr<PCGExData::FFacade>& InFacade, const bool bForward) const
	{
		if (!Details->bSplitUnmatched)
		{
			if (!Details->bQuietUnmatchedTargetWarning) { PCGE_LOG_C(Warning, GraphAndLog, InFacade->GetContext(), FTEXT("An input has no matching target.")); }
		}
		else
		{
			InFacade->Source->OutputPin = Labels::OutputUnmatchedLabel;
		}

		if (bForward && Details->bOutputUnmatched) { InFacade->Source->InitializeOutput(PCGExData::EIOInit::Forward); }
		return true;
	}

	int32 FDataMatcher::GetMatchLimitFor(const FPCGExTaggedData& InDataCandidate) const
	{
		if (!Details->bSplitUnmatched) { return MAX_int32; }

		int32 OutLimit = 0;
		if (!PCGExData::Helpers::TryGetSettingDataValue<int32>(nullptr, InDataCandidate.Data, Details->LimitInput, Details->LimitAttribute, Details->Limit, OutLimit))
		{
			return MAX_int32;
		}
		return OutLimit;
	}

	void FDataMatcher::RegisterTaggedData(FPCGExContext* InContext, const FPCGExTaggedData& InTaggedData)
	{
		if (const int32* ExistingIndex = MatchableSourcesMap.Find(InTaggedData.Data))
		{
			ensure(false); // There should be no duplicate sources
			return;
		}

		const int32 DataIndex = MatchableSources->Num();

		MatchableSourcesMap.Add(InTaggedData.Data, DataIndex);
		PCGExData::FConstPoint& DataPoint = MatchableSourceFirstElements->Emplace_GetRef(nullptr, 0, DataIndex);

		FPCGExTaggedData& TaggedData = MatchableSources->Add_GetRef(InTaggedData);

		if (!TaggedData.Keys)
		{
			if (const UPCGBasePointData* PointData = Cast<UPCGBasePointData>(TaggedData.Data)) { TaggedData.Keys = MakeShared<FPCGAttributeAccessorKeysPointIndices>(PointData); }
			else if (TaggedData.Data->Metadata) { TaggedData.Keys = MakeShared<FPCGAttributeAccessorKeysEntries>(TaggedData.Data->Metadata); }
		}

		NumSources = MatchableSources->Num();
	}

	bool FDataMatcher::InitInternal(FPCGExContext* InContext, const FName InFactoriesLabel)
	{
		if (MatchMode == EPCGExMapMatchMode::Disabled)
		{
			return true;
		}

		if (MatchableSources->IsEmpty())
		{
			MatchMode = EPCGExMapMatchMode::Disabled;
			return false;
		}

		TArray<TObjectPtr<const UPCGExMatchRuleFactoryData>> Factories;
		if (!PCGExFactories::GetInputFactories(InContext, InFactoriesLabel, Factories, {PCGExFactories::EType::MatchRule}))
		{
			MatchMode = EPCGExMapMatchMode::Disabled;
			return false;
		}

		Operations.Reserve(Factories.Num());
		for (const TObjectPtr<const UPCGExMatchRuleFactoryData>& Factory : Factories)
		{
			TSharedPtr<FPCGExMatchRuleOperation> Operation = Factory->CreateOperation(InContext);
			if (!Operation || !Operation->PrepareForMatchableSources(InContext, MatchableSources)) { return false; }
			Operations.Add(Operation);

			if (Factory->BaseConfig.Strictness == EPCGExMatchStrictness::Required) { RequiredOperations.Add(Operation); }
			else { OptionalOperations.Add(Operation); }
		}

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
