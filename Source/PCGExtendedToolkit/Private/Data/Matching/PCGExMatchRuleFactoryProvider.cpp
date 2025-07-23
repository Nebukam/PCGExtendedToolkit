// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Matching/PCGExMatchRuleFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExCreateMatchRule"
#define PCGEX_NAMESPACE CreateMatchRule

bool FPCGExMatchRuleOperation::PrepareForTargets(FPCGExContext* InContext, const TSharedPtr<TArray<PCGExData::FTaggedData>>& InTargets)
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

namespace PCGExMatching
{
	FDataMatcher::FDataMatcher()
	{
		Targets = MakeShared<TArray<PCGExData::FTaggedData>>();
		Elements = MakeShared<TArray<PCGExData::FElement>>();
	}

	void FDataMatcher::SetDetails(const FPCGExMatchingDetails* InDetails)
	{
		Details = InDetails;
		MatchMode = Details->Mode;
	}

	bool FDataMatcher::Init(FPCGExContext* InContext, const TArray<const UPCGData*>& InTargetData, const TArray<TSharedPtr<PCGExData::FTags>>& InTags, const bool bThrowError)
	{
		if (MatchMode == EPCGExMapMatchMode::Disabled) { return true; }

		check(InTargetData.Num() == InTags.Num());

		Targets->Reserve(InTargetData.Num());
		for (int i = 0; i < InTargetData.Num(); i++) { RegisterTaggedData(InContext, PCGExData::FTaggedData(InTargetData[i], InTags[i], nullptr)); }
		return InitInternal(InContext, bThrowError);
	}

	bool FDataMatcher::Init(FPCGExContext* InContext, const TArray<TSharedRef<PCGExData::FFacade>>& InTargetFacades, const bool bThrowError)
	{
		if (MatchMode == EPCGExMapMatchMode::Disabled) { return true; }

		Targets->Reserve(InTargetFacades.Num());
		for (int i = 0; i < InTargetFacades.Num(); i++) { RegisterTaggedData(InContext, InTargetFacades[i]->Source->GetTaggedData()); }
		return InitInternal(InContext, bThrowError);
	}

	bool FDataMatcher::Init(FPCGExContext* InContext, const TArray<TSharedPtr<PCGExData::FFacade>>& InTargetFacades, const bool bThrowError)
	{
		if (MatchMode == EPCGExMapMatchMode::Disabled) { return true; }

		Targets->Reserve(InTargetFacades.Num());
		for (int i = 0; i < InTargetFacades.Num(); i++) { RegisterTaggedData(InContext, InTargetFacades[i]->Source->GetTaggedData()); }
		return InitInternal(InContext, bThrowError);
	}

	bool FDataMatcher::Test(const UPCGData* InTarget, const TSharedPtr<PCGExData::FPointIO>& InDataCandidate) const
	{
		if (MatchMode == EPCGExMapMatchMode::Disabled) { return true; }

		const int32* DataIndexPtr = TargetsMap.Find(InTarget);
		if (!DataIndexPtr) { return false; }

		const int32 DataIndex = *DataIndexPtr;
		const PCGExData::FElement& TargetElement = *(Elements->GetData() + DataIndex);

		if (MatchMode == EPCGExMapMatchMode::All)
		{
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : RequiredOperations) { if (!Op->Test(TargetElement, InDataCandidate)) { return false; } }
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : OptionalOperations) { if (!Op->Test(TargetElement, InDataCandidate)) { return false; } }
		}
		else
		{
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : RequiredOperations) { if (!Op->Test(TargetElement, InDataCandidate)) { return false; } }
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : OptionalOperations) { if (Op->Test(TargetElement, InDataCandidate)) { return true; } }
		}
		return true;
	}

	bool FDataMatcher::Test(const PCGExData::FElement& InTargetElement, const TSharedPtr<PCGExData::FPointIO>& InDataCandidate) const
	{
		if (MatchMode == EPCGExMapMatchMode::Disabled) { return true; }

		if (MatchMode == EPCGExMapMatchMode::All)
		{
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : RequiredOperations) { if (!Op->Test(InTargetElement, InDataCandidate)) { return false; } }
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : OptionalOperations) { if (!Op->Test(InTargetElement, InDataCandidate)) { return false; } }
		}
		else
		{
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : RequiredOperations) { if (!Op->Test(InTargetElement, InDataCandidate)) { return false; } }
			for (const TSharedPtr<FPCGExMatchRuleOperation>& Op : OptionalOperations) { if (Op->Test(InTargetElement, InDataCandidate)) { return true; } }
		}
		return true;
	}

	void FDataMatcher::PopulateIgnoreList(const TSharedPtr<PCGExData::FPointIO>& InDataCandidate, TSet<const UPCGData*>& OutIgnoreList) const
	{
		if (MatchMode == EPCGExMapMatchMode::Disabled) { return; }

		TArray<PCGExData::FTaggedData>& TargetsRef = *Targets.Get();
		for (const PCGExData::FTaggedData& TaggedData : TargetsRef)
		{
			if (!Test(TaggedData.Data, InDataCandidate)) { OutIgnoreList.Add(TaggedData.Data); }
		}
	}

	int32 FDataMatcher::GetMatchingTargets(const TSharedPtr<PCGExData::FPointIO>& InDataCandidate, TArray<int32>& OutMatches) const
	{
		TArray<PCGExData::FTaggedData>& TargetsRef = *Targets.Get();
		OutMatches.Reset(TargetsRef.Num());

		if (MatchMode == EPCGExMapMatchMode::Disabled)
		{
			PCGEx::ArrayOfIndices(OutMatches, TargetsRef.Num());
		}
		else
		{
			for (int i = 0; i < TargetsRef.Num(); i++) { if (Test(TargetsRef[i].Data, InDataCandidate)) { OutMatches.Add(i); } }
		}

		return OutMatches.Num();
	}

	void FDataMatcher::RegisterTaggedData(FPCGExContext* InContext, const PCGExData::FTaggedData& InTaggedData)
	{
		const int32* ExistingIndex = TargetsMap.Find(InTaggedData.Data);

		if (ExistingIndex)
		{
			// TODO : Log duplicate
			return;
		}

		const int32 DataIndex = Targets->Num();

		TargetsMap.Add(InTaggedData.Data, DataIndex);
		PCGExData::FElement& DataPoint = Elements->Emplace_GetRef(0, DataIndex);

		PCGExData::FTaggedData& TaggedData = Targets->Add_GetRef(InTaggedData);

		if (!TaggedData.Keys)
		{
			if (const UPCGBasePointData* PointData = Cast<UPCGBasePointData>(TaggedData.Data)) { TaggedData.Keys = MakeShared<FPCGAttributeAccessorKeysPointIndices>(PointData); }
			else if (TaggedData.Data->Metadata) { TaggedData.Keys = MakeShared<FPCGAttributeAccessorKeysEntries>(TaggedData.Data->Metadata); }
		}
	}

	bool FDataMatcher::InitInternal(FPCGExContext* InContext, const bool bThrowError)
	{
		if (Targets->IsEmpty())
		{
			MatchMode = EPCGExMapMatchMode::Disabled;
			return false;
		}

		TArray<TObjectPtr<const UPCGExMatchRuleFactoryData>> Factories;
		if (!PCGExFactories::GetInputFactories(InContext, PCGExMatching::SourceMatchRulesLabel, Factories, {PCGExFactories::EType::MatchRule}, bThrowError))
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
