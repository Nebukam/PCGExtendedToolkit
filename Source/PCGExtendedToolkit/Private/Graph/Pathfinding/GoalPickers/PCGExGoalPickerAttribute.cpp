// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerAttribute.h"

#include "PCGEx.h"

void UPCGExGoalPickerAttribute::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	const UPCGExGoalPickerAttribute* TypedOther = Cast<UPCGExGoalPickerAttribute>(Other);
	if (TypedOther)
	{
		GoalCount = TypedOther->GoalCount;
		SingleSelector = TypedOther->SingleSelector;
		AttributeSelectors = TypedOther->AttributeSelectors;
	}
}

void UPCGExGoalPickerAttribute::PrepareForData(PCGExData::FFacade* InSeedsDataFacade, PCGExData::FFacade* InGoalsDataFacade)
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
	return PCGExMath::SanitizeIndex(static_cast<int32>(SingleGetter ? SingleGetter->Values[Seed.Index] : -1), MaxGoalIndex, IndexSafety);
}

void UPCGExGoalPickerAttribute::GetGoalIndices(const PCGExData::FPointRef& Seed, TArray<int32>& OutIndices) const
{
	for (PCGExData::FCache<double>* Getter : AttributeGetters)
	{
		if (!Getter) { continue; }
		OutIndices.Add(PCGExMath::SanitizeIndex(static_cast<int32>(Getter->Values[Seed.Index]), MaxGoalIndex, IndexSafety));
	}
}

bool UPCGExGoalPickerAttribute::OutputMultipleGoals() const { return GoalCount != EPCGExGoalPickAttributeAmount::Single; }
