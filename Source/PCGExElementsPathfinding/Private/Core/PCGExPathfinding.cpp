// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExPathfinding.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Clusters/PCGExCluster.h"
#include "GoalPickers/PCGExGoalPicker.h"

namespace PCGExPathfinding
{
	bool FNodePick::ResolveNode(const TSharedRef<PCGExClusters::FCluster>& InCluster, const FPCGExNodeSelectionDetails& SelectionDetails)
	{
		if (Node != nullptr) { return true; }

		const FVector SourcePosition = Point.GetLocation();
		const int32 NodeIndex = InCluster->FindClosestNode(SourcePosition, SelectionDetails.PickingMethod);
		if (NodeIndex == -1) { return false; }
		Node = InCluster->GetNode(NodeIndex);
		if (!SelectionDetails.WithinDistance(SourcePosition, InCluster->GetPos(Node)))
		{
			Node = nullptr;
			return false;
		}
		return true;
	}

	void ProcessGoals(const TSharedPtr<PCGExData::FFacade>& InSeedDataFacade, const UPCGExGoalPicker* GoalPicker, TFunction<void(int32, int32)>&& GoalFunc)
	{
		for (int PointIndex = 0; PointIndex < InSeedDataFacade->Source->GetNum(); PointIndex++)
		{
			const PCGExData::FConstPoint& Seed = InSeedDataFacade->GetInPoint(PointIndex);

			if (GoalPicker->OutputMultipleGoals())
			{
				TArray<int32> GoalIndices;
				GoalPicker->GetGoalIndices(Seed, GoalIndices);
				for (const int32 GoalIndex : GoalIndices)
				{
					if (GoalIndex < 0) { continue; }
					GoalFunc(PointIndex, GoalIndex);
				}
			}
			else
			{
				const int32 GoalIndex = GoalPicker->GetGoalIndex(Seed);
				if (GoalIndex < 0) { continue; }
				GoalFunc(PointIndex, GoalIndex);
			}
		}
	}
}
