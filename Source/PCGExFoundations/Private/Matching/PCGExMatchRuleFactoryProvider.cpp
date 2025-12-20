// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Matching/PCGExMatchRuleFactoryProvider.h"

#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExPointIO.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"
#include "Metadata/Accessors/PCGCustomAccessor.h"

#define LOCTEXT_NAMESPACE "PCGExCreateMatchRule"
#define PCGEX_NAMESPACE CreateMatchRule

PCG_DEFINE_TYPE_INFO(FPCGExDataTypeInfoMatchRule, UPCGExMatchRuleFactoryData)

bool FPCGExMatchRuleOperation::PrepareForTargets(FPCGExContext* InContext, const TSharedPtr<TArray<FPCGExTaggedData>>& InTargets)
{
	Targets = InTargets;
	return true;
}

TSharedPtr<FPCGExMatchRuleOperation> UPCGExMatchRuleFactoryData::CreateOperation(FPCGExContext* InContext) const
{
	return nullptr; // Create shape builder operation
}

UPCGExFactoryData* UPCGExMatchRuleFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	return Super::CreateFactory(InContext, InFactory);
}

PCGExMatching::FMatchingScope::FMatchingScope(const int32 InNumCandidates, const bool bUnlimited)
{
	NumCandidates = InNumCandidates;
	if (bUnlimited) { Counter = -MAX_int32; }
}

void PCGExMatching::FMatchingScope::RegisterMatch()
{
	FPlatformAtomics::InterlockedIncrement(&Counter);
}

void PCGExMatching::FMatchingScope::Invalidate()
{
	FPlatformAtomics::InterlockedExchange(&Valid, false);
}

namespace PCGExMatching
{
	FDataMatcher::FDataMatcher()
	{
		Targets = MakeShared<TArray<FPCGExTaggedData>>();
		Elements = MakeShared<TArray<PCGExData::FConstPoint>>();
	}

	bool FDataMatcher::FindIndex(const UPCGData* InData, int32& OutIndex) const
	{
		const int32* DataIndexPtr = TargetsMap.Find(InData);
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

	bool FDataMatcher::Init(FPCGExContext* InContext, const TArray<const UPCGData*>& InTargetData, const TArray<TSharedPtr<PCGExData::FTags>>& InTags, const bool bThrowError)
	{
		check(Details)

		check(InTargetData.Num() == InTags.Num());

		Targets->Reserve(InTargetData.Num());
		for (int i = 0; i < InTargetData.Num(); i++) { RegisterTaggedData(InContext, FPCGExTaggedData(InTargetData[i], InTags[i], nullptr)); }
		return InitInternal(InContext, SourceMatchRulesLabel);
	}

	bool FDataMatcher::Init(FPCGExContext* InContext, const TArray<TSharedRef<PCGExData::FFacade>>& InTargetFacades, const bool bThrowError)
	{
		check(Details)

		Targets->Reserve(InTargetFacades.Num());

		for (int i = 0; i < InTargetFacades.Num(); i++) { RegisterTaggedData(InContext, InTargetFacades[i]->Source->GetTaggedData()); }
		return InitInternal(InContext, SourceMatchRulesLabel);
	}

	bool FDataMatcher::Init(FPCGExContext* InContext, const TArray<TSharedPtr<PCGExData::FFacade>>& InTargetFacades, const bool bThrowError)
	{
		Targets->Reserve(InTargetFacades.Num());

		for (int i = 0; i < InTargetFacades.Num(); i++) { RegisterTaggedData(InContext, InTargetFacades[i]->Source->GetTaggedData()); }
		return InitInternal(InContext, SourceMatchRulesLabel);
	}

	bool FDataMatcher::Init(FPCGExContext* InContext, const TArray<FPCGExTaggedData>& InTargetDatas, const bool bThrowError)
	{
		Targets->Reserve(InTargetDatas.Num());
		for (const FPCGExTaggedData& TaggedData : InTargetDatas) { RegisterTaggedData(InContext, TaggedData); }
		return InitInternal(InContext, SourceMatchRulesLabel);
	}

	bool FDataMatcher::Init(FPCGExContext* InContext, const TSharedPtr<FDataMatcher>& InOtherMatcher, const FName InFactoriesLabel, const bool bThrowError)
	{
		Targets = InOtherMatcher->Targets;
		Elements = InOtherMatcher->Elements;
		TargetsMap = InOtherMatcher->TargetsMap;

		SetDetails(InOtherMatcher->Details);

		return InitInternal(InContext, InFactoriesLabel);
	}

	bool FDataMatcher::Test(const UPCGData* InTarget, const TSharedPtr<PCGExData::FPointIO>& InDataCandidate, FMatchingScope& InMatchingScope) const
	{
		if (MatchMode == EPCGExMapMatchMode::Disabled || Operations.IsEmpty()) { return true; }

		if (Details->bLimitMatches)
		{
			if (!InMatchingScope.IsValid()) { return false; }
		}

		const int32* DataIndexPtr = TargetsMap.Find(InTarget);
		if (!DataIndexPtr) { return false; }

		const int32 DataIndex = *DataIndexPtr;
		const PCGExData::FConstPoint& TargetElement = *(Elements->GetData() + DataIndex);

		bool bMatch = true;

		if (MatchMode == EPCGExMapMatchMode::All)
		{
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : RequiredOperations) { if (!Op->Test(TargetElement, InDataCandidate, InMatchingScope)) { bMatch = false; } }
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : OptionalOperations) { if (!Op->Test(TargetElement, InDataCandidate, InMatchingScope)) { bMatch = false; } }
		}
		else
		{
			bMatch = false;
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : OptionalOperations) { if (Op->Test(TargetElement, InDataCandidate, InMatchingScope)) { bMatch = true; } }
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : RequiredOperations) { if (!Op->Test(TargetElement, InDataCandidate, InMatchingScope)) { bMatch = false; } }
		}

		if (bMatch)
		{
			InMatchingScope.RegisterMatch();
			if (InMatchingScope.GetCounter() > GetMatchLimitFor(InDataCandidate)) { InMatchingScope.Invalidate(); }
		}

		return bMatch;
	}

	bool FDataMatcher::Test(const PCGExData::FConstPoint& InTargetElement, const TSharedPtr<PCGExData::FPointIO>& InDataCandidate, FMatchingScope& InMatchingScope) const
	{
		if (MatchMode == EPCGExMapMatchMode::Disabled || Operations.IsEmpty()) { return true; }

		if (Details->bLimitMatches) { if (!InMatchingScope.IsValid()) { return false; } }

		bool bMatch = true;

		if (MatchMode == EPCGExMapMatchMode::All)
		{
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : RequiredOperations) { if (!Op->Test(InTargetElement, InDataCandidate, InMatchingScope)) { bMatch = false; } }
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : OptionalOperations) { if (!Op->Test(InTargetElement, InDataCandidate, InMatchingScope)) { bMatch = false; } }
		}
		else
		{
			bMatch = false;
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : OptionalOperations) { if (Op->Test(InTargetElement, InDataCandidate, InMatchingScope)) { bMatch = true; } }
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : RequiredOperations) { if (!Op->Test(InTargetElement, InDataCandidate, InMatchingScope)) { bMatch = false; } }
		}

		if (bMatch)
		{
			InMatchingScope.RegisterMatch();
			if (InMatchingScope.GetCounter() >= GetMatchLimitFor(InDataCandidate)) { InMatchingScope.Invalidate(); }
		}

		return bMatch;
	}

	bool FDataMatcher::PopulateIgnoreList(const TSharedPtr<PCGExData::FPointIO>& InDataCandidate, FMatchingScope& InMatchingScope, TSet<const UPCGData*>& OutIgnoreList) const
	{
		if (MatchMode == EPCGExMapMatchMode::Disabled) { return true; }

		int32 NumIgnored = 0;
		TArray<FPCGExTaggedData>& TargetsRef = *Targets.Get();
		for (const FPCGExTaggedData& TaggedData : TargetsRef)
		{
			if (!Test(TaggedData.Data, InDataCandidate, InMatchingScope))
			{
				OutIgnoreList.Add(TaggedData.Data);
				NumIgnored++;
			}
		}

		return Targets->Num() != NumIgnored;
	}

	int32 FDataMatcher::GetMatchingTargets(const TSharedPtr<PCGExData::FPointIO>& InDataCandidate, FMatchingScope& InMatchingScope, TArray<int32>& OutMatches) const
	{
		TArray<FPCGExTaggedData>& TargetsRef = *Targets.Get();
		OutMatches.Reset(TargetsRef.Num());

		if (MatchMode == EPCGExMapMatchMode::Disabled)
		{
			PCGExArrayHelpers::ArrayOfIndices(OutMatches, TargetsRef.Num());
		}
		else
		{
			for (int i = 0; i < TargetsRef.Num(); i++) { if (Test(TargetsRef[i].Data, InDataCandidate, InMatchingScope)) { OutMatches.Add(i); } }
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
			InFacade->Source->OutputPin = OutputUnmatchedLabel;
		}

		if (bForward && Details->bOutputUnmatched) { InFacade->Source->InitializeOutput(PCGExData::EIOInit::Forward); }
		return true;
	}

	int32 FDataMatcher::GetMatchLimitFor(const TSharedPtr<PCGExData::FPointIO>& InDataCandidate) const
	{
		if (!Details->bSplitUnmatched) { return MAX_int32; }

		int32 OutLimit = 0;
		if (!PCGExData::Helpers::TryGetSettingDataValue<int32>(InDataCandidate, Details->LimitInput, Details->LimitAttribute, Details->Limit, OutLimit))
		{
			return MAX_int32;
		}
		return OutLimit;
	}

	void FDataMatcher::RegisterTaggedData(FPCGExContext* InContext, const FPCGExTaggedData& InTaggedData)
	{
		const int32* ExistingIndex = TargetsMap.Find(InTaggedData.Data);

		if (ExistingIndex)
		{
			// TODO : Log duplicate
			return;
		}

		const int32 DataIndex = Targets->Num();

		TargetsMap.Add(InTaggedData.Data, DataIndex);
		PCGExData::FConstPoint& DataPoint = Elements->Emplace_GetRef(nullptr, 0, DataIndex);

		FPCGExTaggedData& TaggedData = Targets->Add_GetRef(InTaggedData);

		if (!TaggedData.Keys)
		{
			if (const UPCGBasePointData* PointData = Cast<UPCGBasePointData>(TaggedData.Data)) { TaggedData.Keys = MakeShared<FPCGAttributeAccessorKeysPointIndices>(PointData); }
			else if (TaggedData.Data->Metadata) { TaggedData.Keys = MakeShared<FPCGAttributeAccessorKeysEntries>(TaggedData.Data->Metadata); }
		}
	}

	bool FDataMatcher::InitInternal(FPCGExContext* InContext, const FName InFactoriesLabel)
	{
		if (MatchMode == EPCGExMapMatchMode::Disabled)
		{
			return true;
		}

		if (Targets->IsEmpty())
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
			if (!Operation || !Operation->PrepareForTargets(InContext, Targets)) { return false; }
			Operations.Add(Operation);

			if (Factory->BaseConfig.Strictness == EPCGExMatchStrictness::Required) { RequiredOperations.Add(Operation); }
			else { OptionalOperations.Add(Operation); }
		}

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
