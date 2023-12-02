// Fill out your copyright notice in the Description page of Project Settings.


#include "Graph/Pathfinding/GoalPickers/PCGExGoalPicker.h"

#include "Data/PCGPointData.h"

void UPCGExGoalPicker::PrepareForData(const UPCGPointData* InSeeds, const UPCGPointData* InGoals)
{
	MaxGoalIndex = InGoals->GetPoints().Num() - 1;
}

int32 UPCGExGoalPicker::GetGoalIndex(const FPCGPoint& Seed, const int32 SeedIndex) const
{
	return PCGEx::SanitizeIndex(SeedIndex, MaxGoalIndex, IndexSafety);
}

void UPCGExGoalPicker::GetGoalIndices(const FPCGPoint& Seed, TArray<int32>& OutIndices) const
{
}

bool UPCGExGoalPicker::OutputMultipleGoals() const { return false; }
