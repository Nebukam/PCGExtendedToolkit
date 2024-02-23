// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Search/PCGExSearchContours.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/PCGExPathfinding.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"

void UPCGExSearchContours::PreprocessCluster(PCGExCluster::FCluster* Cluster)
{
	Super::PreprocessCluster(Cluster);
	Cluster->ProjectNodes(ProjectionSettings);
}

bool UPCGExSearchContours::FindPath(
	const PCGExCluster::FCluster* Cluster,
	const FVector& SeedPosition,
	const FVector& GoalPosition,
	const UPCGExHeuristicOperation* Heuristics,
	const FPCGExHeuristicModifiersSettings* Modifiers,
	TArray<int32>& OutPath,
	PCGExPathfinding::FExtraWeights* ExtraWeights)
{
	const FVector SeedPositionProj = ProjectionSettings.Project(SeedPosition);
	const FVector GoalPositionProj = ProjectionSettings.Project(GoalPosition);

	const int32 StartNodeIndex = Cluster->FindClosestNode(SeedPositionProj, SearchMode, 2);
	const int32 EndNodeIndex = Cluster->FindClosestNode(GoalPositionProj, SearchMode, 1);

	if (StartNodeIndex == EndNodeIndex || StartNodeIndex == -1 || EndNodeIndex == -1) { return false; }

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExSearchContours::FindContours);

	const FVector InitialDir = PCGExMath::GetNormal(Cluster->Nodes[StartNodeIndex].Position, SeedPositionProj, SeedPositionProj + FVector::UpVector);
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
	FVector DirToPreviousIndex = Cluster->GetEdgeDirection(NextIndex, PreviousIndex);
	PreviousIndex = NextIndex;
	NextIndex = Cluster->FindClosestNeighborLeft(NextIndex, DirToPreviousIndex, Exclusion, 2);


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

		DirToPreviousIndex = Cluster->GetEdgeDirection(NextIndex, PreviousIndex);
		PreviousIndex = NextIndex;
		NextIndex = Cluster->FindClosestNeighborLeft(NextIndex, DirToPreviousIndex, Exclusion, 1);
	}

	OutPath.Add(EndIndex);

	return true;
}
