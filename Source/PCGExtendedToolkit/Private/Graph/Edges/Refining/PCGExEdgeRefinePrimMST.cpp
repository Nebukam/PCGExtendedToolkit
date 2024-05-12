// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Refining/PCGExEdgeRefinePrimMST.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicLocalDistance.h"
#include "Graph/Pathfinding/Search/PCGExScoredQueue.h"

void UPCGExEdgeRefinePrimMST::PrepareForPointIO(PCGExData::FPointIO* InPointIO)
{
	Super::PrepareForPointIO(InPointIO);
}

void UPCGExEdgeRefinePrimMST::PreProcess(PCGExCluster::FCluster* InCluster, PCGExGraph::FGraph* InGraph, PCGExData::FPointIO* InEdgesIO)
{
	Super::PreProcess(InCluster, InGraph, InEdgesIO);

	HeuristicsOperation = NewObject<UPCGExHeuristicLocalDistance>(); // Leak?

	HeuristicsModifiers.PrepareForData(*PointIO, *InEdgesIO);
	HeuristicsOperation->ReferenceWeight = HeuristicsModifiers.ReferenceWeight;
	HeuristicsOperation->PrepareForData(InCluster);
}

void UPCGExEdgeRefinePrimMST::Process(PCGExCluster::FCluster* InCluster, PCGExGraph::FGraph* InGraph, PCGExData::FPointIO* InEdgesIO)
{
	const PCGExCluster::FNode* NoNode = new PCGExCluster::FNode();
	const int32 NumNodes = InCluster->Nodes.Num();

	TSet<int32> Visited;
	Visited.Reserve(NumNodes);

	PCGExSearch::TScoredQueue* ScoredQueue = new PCGExSearch::TScoredQueue(NumNodes, 0, 0);

	TArray<int32> Parent;
	Parent.SetNum(NumNodes);

	for (int i = 0; i < NumNodes; i++)
	{
		ScoredQueue->Scores[i] = TNumericLimits<double>::Max();
		Parent[i] = 0;
	}

	ScoredQueue->Scores[0] = 0;

	int32 CurrentNodeIndex;
	double CurrentNodeScore;
	while (ScoredQueue->Dequeue(CurrentNodeIndex, CurrentNodeScore))
	{
		const PCGExCluster::FNode& Current = InCluster->Nodes[CurrentNodeIndex];
		Visited.Add(CurrentNodeIndex);

		for (const int32 AdjacentIndex : Current.AdjacentNodes)
		{
			if (Visited.Contains(AdjacentIndex)) { continue; } // Exit early

			const PCGExCluster::FNode& AdjacentNode = InCluster->Nodes[AdjacentIndex];
			const PCGExGraph::FIndexedEdge& Edge = InCluster->GetEdgeFromNodeIndices(CurrentNodeIndex, AdjacentIndex);

			double Score = HeuristicsOperation->GetEdgeScore(Current, AdjacentNode, Edge, *NoNode, *NoNode);
			Score += HeuristicsModifiers.GetScore(AdjacentNode.PointIndex, Edge.PointIndex);

			if (Score >= ScoredQueue->Scores[AdjacentIndex]) { continue; }

			ScoredQueue->Scores[AdjacentIndex] = Score;
			Parent[AdjacentIndex] = CurrentNodeIndex;

			ScoredQueue->Enqueue(AdjacentIndex, Score);
		}
	}

	PCGExGraph::FIndexedEdge InsertedEdge;
	for (int32 i = 0; i < NumNodes; i++)
	{
		InGraph->InsertEdge(InCluster->Nodes[Parent[i]].PointIndex, InCluster->Nodes[i].PointIndex, InsertedEdge);
	}

	Visited.Empty();
	Parent.Empty();
	PCGEX_DELETE(ScoredQueue)
	PCGEX_DELETE(NoNode)
}

void UPCGExEdgeRefinePrimMST::Cleanup()
{
	HeuristicsModifiers.Cleanup();
	Super::Cleanup();
}
