// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Search/PCGExSearchAStar.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/PCGExPathfinding.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"
#include "Graph/Pathfinding/Search/PCGExScoredQueue.h"

bool UPCGExSearchAStar::FindPath(
	const FVector& SeedPosition,
	const FPCGExNodeSelectionSettings* SeedSelection,
	const FVector& GoalPosition,
	const FPCGExNodeSelectionSettings* GoalSelection,
	const UPCGExHeuristicOperation* Heuristics,
	const FPCGExHeuristicModifiersSettings* Modifiers, TArray<int32>& OutPath, PCGExPathfinding::FExtraWeights* ExtraWeights)
{
	const PCGExCluster::FNode& SeedNode = Cluster->Nodes[Cluster->FindClosestNode(SeedPosition, SeedSelection->PickingMethod, 1)];
	if (!SeedSelection->WithinDistance(SeedNode.Position, SeedPosition)) { return false; }

	const PCGExCluster::FNode& GoalNode = Cluster->Nodes[Cluster->FindClosestNode(GoalPosition, GoalSelection->PickingMethod, 1)];
	if (!GoalSelection->WithinDistance(GoalNode.Position, GoalPosition)) { return false; }

	if (SeedNode.NodeIndex == GoalNode.NodeIndex) { return false; }

	const int32 NumNodes = Cluster->Nodes.Num();

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExSearchAStar::FindPath);

	// Basic A* implementation TODO:Optimize

	TSet<int32> Visited;
	TArray<int32> Previous;
	TArray<double> GScore;

	GScore.SetNum(NumNodes);
	Previous.SetNum(NumNodes);
	for (int i = 0; i < NumNodes; i++)
	{
		GScore[i] = -1;
		Previous[i] = -1;
	}

	double MinGScore = TNumericLimits<double>::Max();
	double MaxGScore = TNumericLimits<double>::Min();

	for (int i = 0; i < Cluster->Nodes.Num(); i++)
	{
		const double GS = Heuristics->GetGlobalScore(Cluster->Nodes[i], SeedNode, GoalNode);
		MinGScore = FMath::Min(MinGScore, GS);
		MaxGScore = FMath::Max(MaxGScore, GS);
	}

	PCGExSearch::TScoredQueue* ScoredQueue = new PCGExSearch::TScoredQueue(
		NumNodes, SeedNode.NodeIndex, Heuristics->GetGlobalScore(SeedNode, SeedNode, GoalNode));
	GScore[SeedNode.NodeIndex] = 0;

	bool bSuccess = false;

	int32 CurrentNodeIndex;
	double CurrentFScore;
	while (ScoredQueue->Dequeue(CurrentNodeIndex, CurrentFScore))
	{
		if (CurrentNodeIndex == GoalNode.NodeIndex) { break; } // Exit early

		const double CurrentGScore = GScore[CurrentNodeIndex];
		const PCGExCluster::FNode& Current = Cluster->Nodes[CurrentNodeIndex];
		Visited.Add(CurrentNodeIndex);

		for (const int32 AdjacentIndex : Current.AdjacentNodes)
		{
			if (Visited.Contains(AdjacentIndex)) { continue; }

			const PCGExCluster::FNode& AdjacentNode = Cluster->Nodes[AdjacentIndex];
			const PCGExGraph::FIndexedEdge& Edge = Cluster->GetEdgeFromNodeIndices(CurrentNodeIndex, AdjacentIndex);

			const double ExtraWeight = +ExtraWeights ? ExtraWeights->GetExtraWeight(CurrentNodeIndex, Edge.EdgeIndex) : 0;
			const double ScoreMod = Modifiers->GetScore(AdjacentNode.PointIndex, Edge.PointIndex);
			const double TentativeGScore = CurrentGScore + Heuristics->GetEdgeScore(Current, AdjacentNode, Edge, SeedNode, GoalNode) + ScoreMod;

			const double PreviousGScore = GScore[AdjacentIndex];
			if (PreviousGScore != -1 && TentativeGScore >= PreviousGScore) { continue; }

			Previous[AdjacentIndex] = CurrentNodeIndex;
			GScore[AdjacentIndex] = TentativeGScore;

			const double GS = PCGExMath::Remap(Heuristics->GetGlobalScore(AdjacentNode, SeedNode, GoalNode), MinGScore, MaxGScore, 0, 1);
			const double FScore = TentativeGScore + GS * Heuristics->ReferenceWeight;

			ScoredQueue->Enqueue(AdjacentIndex, FScore);
		}
	}

	if (int32 PathIndex = Previous[GoalNode.NodeIndex];
		PathIndex != -1)
	{
		PathIndex = GoalNode.NodeIndex;

		bSuccess = true;
		TArray<int32> Path;
		if (ExtraWeights)
		{
			const double ExtraNodeWeight = Heuristics->ReferenceWeight * ExtraWeights->NodeScale;
			const double ExtraEdgeWeight = Heuristics->ReferenceWeight * ExtraWeights->EdgeScale;
			while (PathIndex != -1)
			{
				const int32 CurrentIndex = PathIndex;
				ExtraWeights->AddPointWeight(CurrentIndex, ExtraNodeWeight);
				Path.Add(CurrentIndex);
				PathIndex = Previous[PathIndex];

				if (PathIndex != -1)
				{
					const PCGExGraph::FIndexedEdge& Edge = Cluster->GetEdgeFromNodeIndices(CurrentIndex, PathIndex);
					ExtraWeights->AddEdgeWeight(Edge.EdgeIndex, ExtraEdgeWeight);
				}
			}
		}
		else
		{
			while (PathIndex != -1)
			{
				Path.Add(PathIndex);
				PathIndex = Previous[PathIndex];
			}
		}
		Algo::Reverse(Path);
		OutPath.Append(Path);
	}

	PCGEX_DELETE(ScoredQueue)
	Previous.Empty();
	GScore.Empty();

	return bSuccess;
}
