// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Refining/PCGExEdgeRefinePrimMST.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicLocalDistance.h"

void UPCGExEdgeRefinePrimMST::PrepareForPointIO(PCGExData::FPointIO* InPointIO)
{
	Super::PrepareForPointIO(InPointIO);
}

void UPCGExEdgeRefinePrimMST::Process(PCGExCluster::FCluster* InCluster, PCGExGraph::FGraph* InGraph, PCGExData::FPointIO* InEdgesIO)
{
	const TObjectPtr<UPCGExHeuristicLocalDistance> Heuristics = NewObject<UPCGExHeuristicLocalDistance>();

	HeuristicsModifiers.PrepareForData(*PointIO, *InEdgesIO);
	Heuristics->ReferenceWeight = HeuristicsModifiers.Scale;
	Heuristics->PrepareForData(InCluster);

	const PCGExCluster::FNode* NoNode = new PCGExCluster::FNode();
	const int32 NumNodes = InCluster->Nodes.Num();

	TArray<PCGExCluster::FScoredNode*> OpenList;
	TSet<int32> VisitedNodes;

	VisitedNodes.Reserve(NumNodes);
	OpenList.Reserve(NumNodes);

	TArray<double> BestScore;
	TArray<int32> Parent;

	BestScore.SetNum(NumNodes);
	Parent.SetNum(NumNodes);

	for (int i = 0; i < NumNodes; i++)
	{
		BestScore[i] = TNumericLimits<double>::Max();
		Parent[i] = 0;
	}

	// Set the key of the first vertex to 0 and push it to the priority queue
	OpenList.Add(new PCGExCluster::FScoredNode(InCluster->Nodes[0], 0)); // weight, vertex

	while (!OpenList.IsEmpty())
	{
		// Extract the vertex with the minimum key from the priority queue
		const PCGExCluster::FScoredNode* CurrentScoredNode = OpenList.Pop();
		int32 CurrentNodeIndex = CurrentScoredNode->Node->NodeIndex;

		VisitedNodes.Add(CurrentNodeIndex);

		// Explore all adjacent vertices of the extracted vertex
		for (const int32 EdgeIndex : CurrentScoredNode->Node->Edges)
		{
			PCGExGraph::FIndexedEdge Edge = InCluster->Edges[EdgeIndex];
			const PCGExCluster::FNode& AdjacentNode = InCluster->GetNodeFromPointIndex(Edge.Other(CurrentScoredNode->Node->PointIndex));

			const int32 AdjacentIndex = AdjacentNode.NodeIndex;

			if (VisitedNodes.Contains(AdjacentIndex)) { continue; }

			double Score = Heuristics->ComputeDScore(*CurrentScoredNode->Node, AdjacentNode, Edge, *NoNode, *NoNode);
			Score += HeuristicsModifiers.GetScore(AdjacentNode.PointIndex, Edge.EdgeIndex);

			if (Score < BestScore[AdjacentIndex])
			{
				// Update the minimum weight of the adjacent vertex and push it to the priority queue
				BestScore[AdjacentIndex] = Score;
				Parent[AdjacentIndex] = CurrentNodeIndex;

				Heuristics->ScoredInsert(OpenList, new PCGExCluster::FScoredNode(AdjacentNode, Score));
			}
		}

		PCGEX_DELETE(CurrentScoredNode)
	}

	PCGExGraph::FIndexedEdge InsertedEdge;
	for (int32 i = 0; i < NumNodes; i++) { InGraph->InsertEdge(InCluster->Nodes[Parent[i]].PointIndex, InCluster->Nodes[i].PointIndex, InsertedEdge); }

	VisitedNodes.Empty();
	PCGEX_DELETE_TARRAY(OpenList)
	PCGEX_DELETE(NoNode)
}

void UPCGExEdgeRefinePrimMST::Cleanup()
{
	HeuristicsModifiers.Cleanup();
	Super::Cleanup();
}
