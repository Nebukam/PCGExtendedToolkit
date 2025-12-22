// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Core/PCGExMT.h"
#include "Data/PCGExPointElements.h"

namespace PCGExPathfinding
{
	struct FSeedGoalPair;
}

class FPCGExPathfindingTask : public PCGExMT::FPCGExIndexedTask
{
public:
	FPCGExPathfindingTask(const int32 InTaskIndex, const TSharedPtr<PCGExData::FPointIO>& InPointIO, const TArray<PCGExPathfinding::FSeedGoalPair>* InQueries)
		: FPCGExIndexedTask(InTaskIndex), PointIO(InPointIO), Queries(InQueries)
	{
	}

	TSharedPtr<PCGExData::FPointIO> PointIO;
	const TArray<PCGExPathfinding::FSeedGoalPair>* Queries = nullptr;
};
