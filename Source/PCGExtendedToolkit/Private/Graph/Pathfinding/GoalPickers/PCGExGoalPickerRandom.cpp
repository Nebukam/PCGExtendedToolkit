// Fill out your copyright notice in the Description page of Project Settings.


#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"

#include "PCGExMath.h"
#include "Data/PCGPointData.h"

int32 UPCGExGoalPickerRandom::GetGoalIndex(const FPCGPoint& Seed, const int32 SeedIndex) const
{
	const int32 Index = static_cast<int32>(PCGExMath::Remap(
		FMath::PerlinNoise3D(PCGExMath::CWWrap(Seed.Transform.GetLocation() * 0.001, FVector(-1), FVector(1))),
		-1, 1, 0, MaxGoalIndex));
	return PCGEx::SanitizeIndex(Index, MaxGoalIndex, IndexSafety);
}

void UPCGExGoalPickerRandom::GetGoalIndices(const FPCGPoint& Seed, TArray<int32>& OutIndices) const
{
	int32 Picks = GoalCount == EPCGExGoalPickRandomAmount::Random ?
		              PCGExMath::Remap(
			              FMath::PerlinNoise3D(PCGExMath::CWWrap(Seed.Transform.GetLocation() * 0.001 + NumGoals, FVector(-1), FVector(1))),
			              -1, 1, 0, NumGoals) : NumGoals;

	Picks = FMath::Min(1, FMath::Min(Picks, MaxGoalIndex));

	for (int i = 0; i < Picks; i++)
	{
		int32 Index = static_cast<int32>(PCGExMath::Remap(
			FMath::PerlinNoise3D(PCGExMath::CWWrap(Seed.Transform.GetLocation() * 0.001 + i, FVector(-1), FVector(1))),
			-1, 1, 0, MaxGoalIndex));
		OutIndices.Add(PCGEx::SanitizeIndex(Index, MaxGoalIndex, IndexSafety));
	}
}

bool UPCGExGoalPickerRandom::OutputMultipleGoals() const { return GoalCount != EPCGExGoalPickRandomAmount::Single; }
