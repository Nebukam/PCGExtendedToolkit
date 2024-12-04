// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerAttribute.h"


void UPCGExGoalPickerAttribute::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExGoalPickerAttribute* TypedOther = Cast<UPCGExGoalPickerAttribute>(Other))
	{
		GoalCount = TypedOther->GoalCount;
		SingleSelector = TypedOther->SingleSelector;
		AttributeSelectors = TypedOther->AttributeSelectors;

		if (!TypedOther->CommaSeparatedNames.IsEmpty())
		{
			for (const TArray<FString> Names = PCGExHelpers::GetStringArrayFromCommaSeparatedList(CommaSeparatedNames);
			     const FString& Name : Names)
			{
				FPCGAttributePropertyInputSelector NewSelector = FPCGAttributePropertyInputSelector();
				NewSelector.Update(Name);
				AttributeSelectors.AddUnique(NewSelector);
			}
		}
	}
}

void UPCGExGoalPickerAttribute::PrepareForData(const TSharedPtr<PCGExData::FFacade>& InSeedsDataFacade, const TSharedPtr<PCGExData::FFacade>& InGoalsDataFacade)
{
	Super::PrepareForData(InSeedsDataFacade, InGoalsDataFacade);

	if (GoalCount == EPCGExGoalPickAttributeAmount::Single)
	{
		SingleGetter = InSeedsDataFacade->GetBroadcaster<double>(SingleSelector);
	}
	else
	{
		AttributeGetters.Reset(AttributeSelectors.Num());
		for (const FPCGAttributePropertyInputSelector& Selector : AttributeSelectors) { AttributeGetters.Add(InSeedsDataFacade->GetBroadcaster<double>(Selector)); }
	}
}

int32 UPCGExGoalPickerAttribute::GetGoalIndex(const PCGExData::FPointRef& Seed) const
{
	return PCGExMath::SanitizeIndex(static_cast<int32>(SingleGetter ? SingleGetter->Read(Seed.Index) : -1), MaxGoalIndex, IndexSafety);
}

void UPCGExGoalPickerAttribute::GetGoalIndices(const PCGExData::FPointRef& Seed, TArray<int32>& OutIndices) const
{
	for (const TSharedPtr<PCGExData::TBuffer<double>>& Getter : AttributeGetters)
	{
		if (!Getter) { continue; }
		OutIndices.Add(PCGExMath::SanitizeIndex(static_cast<int32>(Getter->Read(Seed.Index)), MaxGoalIndex, IndexSafety));
	}
}

bool UPCGExGoalPickerAttribute::OutputMultipleGoals() const { return GoalCount != EPCGExGoalPickAttributeAmount::Single; }
