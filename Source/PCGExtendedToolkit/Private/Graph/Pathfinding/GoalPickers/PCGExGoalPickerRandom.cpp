// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"

#include "PCGExMath.h"


void UPCGExGoalPickerRandom::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExGoalPickerRandom* TypedOther = Cast<UPCGExGoalPickerRandom>(Other))
	{
		GoalCount = TypedOther->GoalCount;
		NumGoals = TypedOther->NumGoals;
		bUseNumGoalsAttribute = TypedOther->bUseNumGoalsAttribute;
		NumGoalAttribute = TypedOther->NumGoalAttribute;
	}
}

bool UPCGExGoalPickerRandom::PrepareForData(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InSeedsDataFacade, const TSharedPtr<PCGExData::FFacade>& InGoalsDataFacade)
{
	if (!Super::PrepareForData(InContext, InSeedsDataFacade, InGoalsDataFacade)) { return false; }

	if (bUseNumGoalsAttribute)
	{
		NumGoalsGetter = InSeedsDataFacade->GetBroadcaster<int32>(NumGoalAttribute);
		if (!NumGoalsGetter)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Invalid NumGoals selector on Seeds: \"{0}\"."), FText::FromString(PCGEx::GetSelectorDisplayName(NumGoalAttribute))));
			return false;
		}
	}
	return true;
}

int32 UPCGExGoalPickerRandom::GetGoalIndex(const PCGExData::FPointRef& Seed) const
{
	const int32 Index = static_cast<int32>(PCGExMath::Remap(
		FMath::PerlinNoise3D(PCGExMath::Tile(Seed.Point->Transform.GetLocation() * 0.001, FVector(-1), FVector(1))),
		-1, 1, 0, MaxGoalIndex));
	return PCGExMath::SanitizeIndex(Index, MaxGoalIndex, IndexSafety);
}

void UPCGExGoalPickerRandom::GetGoalIndices(const PCGExData::FPointRef& Seed, TArray<int32>& OutIndices) const
{
	int32 Picks = NumGoalsGetter ? NumGoalsGetter->Read(Seed.Index) : NumGoals;

	if (GoalCount == EPCGExGoalPickRandomAmount::Random)
	{
		Picks = PCGExMath::Remap(
			FMath::PerlinNoise3D(PCGExMath::Tile(Seed.Point->Transform.GetLocation() * 0.001 + Picks, FVector(-1), FVector(1))),
			-1, 1, 0, Picks);
	}

	Picks = FMath::Min(1, FMath::Min(Picks, MaxGoalIndex));

	for (int i = 0; i < Picks; i++)
	{
		int32 Index = static_cast<int32>(PCGExMath::Remap(
			FMath::PerlinNoise3D(PCGExMath::Tile(Seed.Point->Transform.GetLocation() * 0.001 + i, FVector(-1), FVector(1))),
			-1, 1, 0, MaxGoalIndex));
		OutIndices.Add(PCGExMath::SanitizeIndex(Index, MaxGoalIndex, IndexSafety));
	}
}

bool UPCGExGoalPickerRandom::OutputMultipleGoals() const { return GoalCount != EPCGExGoalPickRandomAmount::Single; }

void UPCGExGoalPickerRandom::Cleanup()
{
	Super::Cleanup();
}
