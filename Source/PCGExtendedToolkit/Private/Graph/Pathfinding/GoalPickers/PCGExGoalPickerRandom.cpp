// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"

#include "PCGExMath.h"
#include "Data/PCGPointData.h"

int32 UPCGExGoalPickerRandom::GetGoalIndex(const PCGEx::FPointRef& Seed) const
{
	const int32 Index = static_cast<int32>(PCGExMath::Remap(
		FMath::PerlinNoise3D(PCGExMath::CWWrap(Seed.Point->Transform.GetLocation() * 0.001, FVector(-1), FVector(1))),
		-1, 1, 0, MaxGoalIndex));
	return PCGEx::SanitizeIndex(Index, MaxGoalIndex, IndexSafety);
}

void UPCGExGoalPickerRandom::GetGoalIndices(const PCGEx::FPointRef& Seed, TArray<int32>& OutIndices) const
{
	int32 Picks = GoalCount == EPCGExGoalPickRandomAmount::Random ?
		              PCGExMath::Remap(
			              FMath::PerlinNoise3D(PCGExMath::CWWrap(Seed.Point->Transform.GetLocation() * 0.001 + NumGoals, FVector(-1), FVector(1))),
			              -1, 1, 0, NumGoals) : NumGoals;

	Picks = FMath::Min(1, FMath::Min(Picks, MaxGoalIndex));

	for (int i = 0; i < Picks; i++)
	{
		int32 Index = static_cast<int32>(PCGExMath::Remap(
			FMath::PerlinNoise3D(PCGExMath::CWWrap(Seed.Point->Transform.GetLocation() * 0.001 + i, FVector(-1), FVector(1))),
			-1, 1, 0, MaxGoalIndex));
		OutIndices.Add(PCGEx::SanitizeIndex(Index, MaxGoalIndex, IndexSafety));
	}
}

bool UPCGExGoalPickerRandom::OutputMultipleGoals() const { return GoalCount != EPCGExGoalPickRandomAmount::Single; }
