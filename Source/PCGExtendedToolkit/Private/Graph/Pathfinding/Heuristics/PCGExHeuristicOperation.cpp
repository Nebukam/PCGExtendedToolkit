// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"

#include "Data/PCGPointData.h"


void UPCGExHeuristicOperation::PrepareForData(const PCGExMesh::FMesh* InMesh)
{
}

double UPCGExHeuristicOperation::ComputeScore(
	const PCGExMesh::FScoredVertex* From,
	const PCGExMesh::FVertex& To,
	const PCGExMesh::FVertex& Seed,
	const PCGExMesh::FVertex& Goal,
	const PCGExMesh::FIndexedEdge& Edge) const
{
	return From->Score + 1;
}

bool UPCGExHeuristicOperation::IsBetterScore(const double NewScore, const double OtherScore) const
{
	return NewScore <= OtherScore;
}

int32 UPCGExHeuristicOperation::GetQueueingIndex(const TArray<PCGExMesh::FScoredVertex*>& InVertices, const double InScore) const
{
	for (int i = InVertices.Num() - 1; i >= 0; i--)
	{
		if (IsBetterScore(InScore, InVertices[i]->Score)) { return i + 1; }
	}
	return -1;
}
