// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "GoalPickers/PCGExGoalPickerRandom.h"

#include "Helpers/PCGExRandomHelpers.h"
#include "Data/PCGExPointElements.h"
#include "Details/PCGExSettingsDetails.h"

PCGEX_SETTING_VALUE_IMPL(UPCGExGoalPickerRandom, NumGoals, int32, NumGoalsType, NumGoalAttribute, NumGoals)

void UPCGExGoalPickerRandom::CopySettingsFrom(const UPCGExInstancedFactory* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExGoalPickerRandom* TypedOther = Cast<UPCGExGoalPickerRandom>(Other))
	{
		LocalSeed = TypedOther->LocalSeed;
		GoalCount = TypedOther->GoalCount;
		NumGoalsType = TypedOther->NumGoalsType;
		NumGoals = TypedOther->NumGoals;
		NumGoalAttribute = TypedOther->NumGoalAttribute;
	}
}

bool UPCGExGoalPickerRandom::PrepareForData(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InSeedsDataFacade, const TSharedPtr<PCGExData::FFacade>& InGoalsDataFacade)
{
	if (!Super::PrepareForData(InContext, InSeedsDataFacade, InGoalsDataFacade)) { return false; }

	NumGoalsBuffer = GetValueSettingNumGoals();
	if (!NumGoalsBuffer->Init(InSeedsDataFacade, false)) { return false; }

	return true;
}

int32 UPCGExGoalPickerRandom::GetGoalIndex(const PCGExData::FConstPoint& Seed) const
{
	FRandomStream Random = PCGExRandomHelpers::GetRandomStreamFromPoint(Seed.Data->GetSeed(Seed.Index), Seed.Index);
	return Random.RandRange(0, MaxGoalIndex);
}

void UPCGExGoalPickerRandom::GetGoalIndices(const PCGExData::FConstPoint& Seed, TArray<int32>& OutIndices) const
{
	int32 Picks = NumGoalsBuffer->Read(Seed.Index);

	FRandomStream Random = PCGExRandomHelpers::GetRandomStreamFromPoint(Seed.Data->GetSeed(Seed.Index), Seed.Index);

	if (GoalCount == EPCGExGoalPickRandomAmount::Random) { Picks = Random.RandRange(0, Picks); }

	for (int i = 0; i < Picks; i++) { OutIndices.Emplace(Random.RandRange(0, MaxGoalIndex)); }
}

bool UPCGExGoalPickerRandom::OutputMultipleGoals() const { return GoalCount != EPCGExGoalPickRandomAmount::Single; }

void UPCGExGoalPickerRandom::Cleanup()
{
	Super::Cleanup();
}
