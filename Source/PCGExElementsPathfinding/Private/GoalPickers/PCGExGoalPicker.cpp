// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "GoalPickers/PCGExGoalPicker.h"

#include "Data/PCGExPointElements.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"


void UPCGExGoalPicker::CopySettingsFrom(const UPCGExInstancedFactory* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExGoalPicker* TypedOther = Cast<UPCGExGoalPicker>(Other))
	{
		IndexSafety = TypedOther->IndexSafety;
	}
}

bool UPCGExGoalPicker::PrepareForData(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InSeedsDataFacade, const TSharedPtr<PCGExData::FFacade>& InGoalsDataFacade)
{
	MaxGoalIndex = InGoalsDataFacade->Source->GetNum() - 1;
	if (MaxGoalIndex < 0)
	{
		PCGEX_LOG_MISSING_INPUT(Context, FTEXT("Missing goal points."))
		return false;
	}
	return true;
}

int32 UPCGExGoalPicker::GetGoalIndex(const PCGExData::FConstPoint& Seed) const
{
	return PCGExMath::SanitizeIndex(Seed.Index, MaxGoalIndex, IndexSafety);
}

void UPCGExGoalPicker::GetGoalIndices(const PCGExData::FConstPoint& Seed, TArray<int32>& OutIndices) const
{
}

bool UPCGExGoalPicker::OutputMultipleGoals() const { return false; }
