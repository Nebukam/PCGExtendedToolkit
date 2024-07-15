// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/GoalPickers/PCGExGoalPicker.h"

#include "Data/PCGExPointIO.h"

void UPCGExGoalPicker::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	const UPCGExGoalPicker* TypedOther = Cast<UPCGExGoalPicker>(Other);
	if (TypedOther)
	{
		IndexSafety = TypedOther->IndexSafety;
	}
}

void UPCGExGoalPicker::PrepareForData(PCGExData::FFacade* InSeedsDataFacade, PCGExData::FFacade* InGoalsDataFacade)
{
	MaxGoalIndex = InGoalsDataFacade->Source->GetNum() - 1;
}

int32 UPCGExGoalPicker::GetGoalIndex(const PCGExData::FPointRef& Seed) const
{
	return PCGExMath::SanitizeIndex(Seed.Index, MaxGoalIndex, IndexSafety);
}

void UPCGExGoalPicker::GetGoalIndices(const PCGExData::FPointRef& Seed, TArray<int32>& OutIndices) const
{
}

bool UPCGExGoalPicker::OutputMultipleGoals() const { return false; }
