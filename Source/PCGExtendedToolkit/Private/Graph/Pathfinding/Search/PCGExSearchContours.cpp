// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Search/PCGExSearchContours.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

bool UPCGExSearchContours::GetRequiresProjection() { return true; }

bool UPCGExSearchContours::FindPath(
	const FVector& SeedPosition,
	const FPCGExNodeSelectionSettings* SeedSelection,
	const FVector& GoalPosition,
	const FPCGExNodeSelectionSettings* GoalSelection,
	PCGExHeuristics::THeuristicsHandler* Heuristics,
	TArray<int32>& OutPath, PCGExHeuristics::FLocalFeedbackHandler* LocalFeedback)
{
	const int32 StartNodeIndex = Cluster->FindClosestNode(SeedPosition, SeedSelection->PickingMethod, 2);
	const int32 EndNodeIndex = Cluster->FindClosestNode(GoalPosition, GoalSelection->PickingMethod, 1);

	if (StartNodeIndex == EndNodeIndex || StartNodeIndex == -1 || EndNodeIndex == -1) { return false; }

	if (!SeedSelection->WithinDistance(Cluster->Nodes[StartNodeIndex].Position, SeedPosition)) { return false; }
	if (!GoalSelection->WithinDistance(Cluster->Nodes[EndNodeIndex].Position, GoalPosition)) { return false; }

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExSearchContours::FindContours);

	const FVector InitialDir = PCGExMath::GetNormal(Cluster->Nodes[StartNodeIndex].Position, SeedPosition, SeedPosition + FVector::UpVector);
	const int32 NextToStartIndex = Cluster->FindClosestNeighborInDirection(StartNodeIndex, InitialDir, 2);

	if (NextToStartIndex == -1)
	{
		// Fail. Either single-node or single-edge cluster
		return false;
	}

	int32 PreviousIndex = StartNodeIndex;
	int32 NextIndex = NextToStartIndex;

	OutPath.Add(StartNodeIndex);
	OutPath.Add(NextToStartIndex);

	TSet<int32> Exclusion = {PreviousIndex, NextIndex};
	PreviousIndex = NextToStartIndex;
	NextIndex = Projection->FindNextAdjacentNode(OrientationMode, NextToStartIndex, StartNodeIndex, Exclusion, 2);

	int32 EndIndex = -1;

	while (NextIndex != -1)
	{
		if (NextIndex == EndNodeIndex)
		{
			// Contour closed gracefully
			EndIndex = EndNodeIndex;
			break;
		}

		if (NextIndex == StartNodeIndex)
		{
			// Contour closed by eating its own tail.
			EndIndex = StartNodeIndex;
			break;
		}

		const PCGExCluster::FNode& CurrentNode = Cluster->Nodes[NextIndex];

		OutPath.Add(NextIndex);

		if (CurrentNode.AdjacentNodes.Contains(EndNodeIndex))
		{
			// End is in the immediate vicinity
			EndIndex = EndNodeIndex;
			break;
		}

		if (CurrentNode.AdjacentNodes.Contains(StartNodeIndex))
		{
			// End is in the immediate vicinity
			EndIndex = StartNodeIndex;
			break;
		}

		Exclusion.Empty();
		if (CurrentNode.AdjacentNodes.Num() > 1) { Exclusion.Add(PreviousIndex); }

		const int32 FromIndex = PreviousIndex;
		PreviousIndex = NextIndex;
		NextIndex = Projection->FindNextAdjacentNode(OrientationMode, NextIndex, FromIndex, Exclusion, 1);
	}

	if (EndIndex != -1) { OutPath.Add(EndIndex); }

	return true;
}
