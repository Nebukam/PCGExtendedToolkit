﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/GoalPickers/PCGExGoalPicker.h"

#include "Data/PCGExPointIO.h"

void UPCGExGoalPicker::PrepareForData(const PCGExData::FPointIO& InSeeds, const PCGExData::FPointIO& InGoals)
{
	MaxGoalIndex = InGoals.GetNum() - 1;
}

int32 UPCGExGoalPicker::GetGoalIndex(const PCGEx::FPointRef& Seed) const
{
	return PCGExMath::SanitizeIndex(Seed.Index, MaxGoalIndex, IndexSafety);
}

void UPCGExGoalPicker::GetGoalIndices(const PCGEx::FPointRef& Seed, TArray<int32>& OutIndices) const
{
}

bool UPCGExGoalPicker::OutputMultipleGoals() const { return false; }
