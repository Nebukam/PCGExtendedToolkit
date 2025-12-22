// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExNavmesh.h"
#include "Core/PCGExPathfinding.h"
#include "NavigationSystem.h"
#include "Paths/PCGExPath.h"

namespace PCGExNavmesh
{
	FNavmeshQuery::FNavmeshQuery(const PCGExPathfinding::FSeedGoalPair& InSeedGoalPair)
		: SeedGoalPair(InSeedGoalPair)
	{
	}

	void FNavmeshQuery::FindPath(FPCGExNavmeshContext* InContext)
	{
		if (!SeedGoalPair.IsValid()) { return; }

		UWorld* World = InContext->GetWorld();
		UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);

		if (!NavSys || !NavSys->GetDefaultNavDataInstance()) { return; }

		FPathFindingQuery PathFindingQuery = FPathFindingQuery(World, *NavSys->GetDefaultNavDataInstance(), SeedGoalPair.SeedPosition, SeedGoalPair.GoalPosition, nullptr, nullptr, TNumericLimits<FVector::FReal>::Max(), InContext->bRequireNavigableEndLocation);

		PathFindingQuery.NavAgentProperties = InContext->NavAgentProperties;

		const FPathFindingResult Result = NavSys->FindPathSync(InContext->NavAgentProperties, PathFindingQuery, InContext->PathfindingMode == EPCGExPathfindingNavmeshMode::Regular ? EPathFindingMode::Type::Regular : EPathFindingMode::Type::Hierarchical);


		if (Result.Result == ENavigationQueryResult::Type::Success)
		{
			TArray<FNavPathPoint>& PathPoints = Result.Path->GetPathPoints();
			Positions.Reserve(PathPoints.Num());

			SeedGoalMetrics = PCGExPaths::FPathMetrics(SeedGoalPair.SeedPosition);

			Positions.Add(PathPoints[0].Location);
			SeedGoalMetrics.Add(Positions[0]);

			if (PathPoints.Num() == 1) { return; }

			PCGExPaths::FPathMetrics Metrics = PCGExPaths::FPathMetrics(Positions[0]);

			const int32 LastIndex = PathPoints.Num() - 1;
			for (int i = 1; i < PathPoints.Num(); i++)
			{
				const FVector Location = PathPoints[i].Location;

				if (Metrics.IsLastWithinRange(Location, InContext->FuseDistance))
				{
					if (i == LastIndex) { Positions.Last() = Location; }
					else { continue; }
				}

				Positions.Add(Location);
				SeedGoalMetrics.Add(Location);
			}

			SeedGoalMetrics.Add(SeedGoalPair.GoalPosition);
		}
	}

	void FNavmeshQuery::CopyPositions(const TPCGValueRange<FTransform>& InRange, int32& OutStartIndex, const bool bAddSeed, const bool bAddGoal)
	{
		if (bAddSeed) { InRange[OutStartIndex++].SetLocation(SeedGoalPair.SeedPosition); }
		for (const FVector& Position : Positions) { InRange[OutStartIndex++].SetLocation(Position); }
		if (bAddGoal) { InRange[OutStartIndex++].SetLocation(SeedGoalPair.GoalPosition); }
		Positions.Empty();
	}
}
