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
			for (const TArray<FString> Names = PCGExHelpers::GetStringArrayFromCommaSeparatedList(TypedOther->CommaSeparatedNames);
				 const FString& Name : Names)
			{
				FPCGAttributePropertyInputSelector NewSelector = FPCGAttributePropertyInputSelector();
				NewSelector.Update(Name);
				AttributeSelectors.AddUnique(NewSelector);
			}
		}		
	}
}

bool UPCGExGoalPickerAttribute::PrepareForData(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InSeedsDataFacade, const TSharedPtr<PCGExData::FFacade>& InGoalsDataFacade)
{
	if (!Super::PrepareForData(InContext, InSeedsDataFacade, InGoalsDataFacade)) { return false; }
	
	if (GoalCount == EPCGExGoalPickAttributeAmount::Single)
	{
		SingleGetter = InSeedsDataFacade->GetBroadcaster<int32>(SingleSelector);
		
		if (!SingleGetter)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Invalid Index selector on Seeds: \"{0}\"."), FText::FromString(PCGEx::GetSelectorDisplayName(SingleSelector))));
			return false;
		}
	}
	else
	{
		if (!CommaSeparatedNames.IsEmpty())
		{
			for (const TArray<FString> Names = PCGExHelpers::GetStringArrayFromCommaSeparatedList(CommaSeparatedNames);
				 const FString& Name : Names)
			{
				FPCGAttributePropertyInputSelector NewSelector = FPCGAttributePropertyInputSelector();
				NewSelector.Update(Name);
				AttributeSelectors.AddUnique(NewSelector);
			}
		}
		
		AttributeGetters.Reset(AttributeSelectors.Num());
		for (const FPCGAttributePropertyInputSelector& Selector : AttributeSelectors)
		{
			TSharedPtr<PCGExData::TBuffer<int32>> Getter = InSeedsDataFacade->GetBroadcaster<int32>(Selector);
			if (!Getter)
			{
				PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Invalid Index selector on Seeds: \"{0}\"."), FText::FromString(PCGEx::GetSelectorDisplayName(Selector))));
				return false;
			}
			
			AttributeGetters.Add(Getter);
		}
	}

	return true;
}

int32 UPCGExGoalPickerAttribute::GetGoalIndex(const PCGExData::FPointRef& Seed) const
{
	return PCGExMath::SanitizeIndex(static_cast<int32>(SingleGetter ? SingleGetter->Read(Seed.Index) : -1), MaxGoalIndex, IndexSafety);
}

void UPCGExGoalPickerAttribute::GetGoalIndices(const PCGExData::FPointRef& Seed, TArray<int32>& OutIndices) const
{
	for (const TSharedPtr<PCGExData::TBuffer<int32>>& Getter : AttributeGetters)
	{
		if (!Getter) { continue; }
		OutIndices.Add(PCGExMath::SanitizeIndex(Getter->Read(Seed.Index), MaxGoalIndex, IndexSafety));
	}
}

bool UPCGExGoalPickerAttribute::OutputMultipleGoals() const { return GoalCount != EPCGExGoalPickAttributeAmount::Single; }
