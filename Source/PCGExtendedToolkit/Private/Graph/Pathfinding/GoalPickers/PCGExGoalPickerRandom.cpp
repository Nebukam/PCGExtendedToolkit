// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"

#include "PCGExMath.h"
#include "Data/PCGPointData.h"

void UPCGExGoalPickerRandom::PrepareForData(const PCGExData::FPointIO& InSeeds, const PCGExData::FPointIO& InGoals)
{
	if (bUseLocalNumGoals && !NumGoalsGetter)
	{
		NumGoalsGetter = new PCGEx::FLocalIntegerGetter();
		NumGoalsGetter->Capture(LocalNumGoalAttribute);
	}

	if (NumGoalsGetter) { NumGoalsGetter->Grab(InSeeds); }

	Super::PrepareForData(InSeeds, InGoals);
}

int32 UPCGExGoalPickerRandom::GetGoalIndex(const PCGEx::FPointRef& Seed) const
{
	const int32 Index = static_cast<int32>(PCGExMath::Remap(
		FMath::PerlinNoise3D(PCGExMath::Tile(Seed.Point->Transform.GetLocation() * 0.001, FVector(-1), FVector(1))),
		-1, 1, 0, MaxGoalIndex));
	return PCGExMath::SanitizeIndex(Index, MaxGoalIndex, IndexSafety);
}

void UPCGExGoalPickerRandom::GetGoalIndices(const PCGEx::FPointRef& Seed, TArray<int32>& OutIndices) const
{
	int32 Picks = NumGoalsGetter ? NumGoalsGetter->SafeGet(Seed.Index, NumGoals) : NumGoals;

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
	PCGEX_DELETE(NumGoalsGetter)
	Super::Cleanup();
}

void UPCGExGoalPickerRandom::ApplyOverrides()
{
	Super::ApplyOverrides();
	PCGEX_OVERRIDE_OP_PROPERTY(NumGoals, FName(TEXT("Goals/NumGoals")), EPCGMetadataTypes::Integer32);
	PCGEX_OVERRIDE_OP_PROPERTY(bUseLocalNumGoals, FName(TEXT("Goals/UseLocalNumGoals")), EPCGMetadataTypes::Boolean);
}
