// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Clusters/PCGExNode.h"

#include "Clusters/PCGExCluster.h"

namespace PCGExGraphs
{
	FNode::FNode(const int32 InNodeIndex, const int32 InPointIndex)
		: Index(InNodeIndex), PointIndex(InPointIndex)
	{
		Links.Empty();
	}

	bool FNode::IsAdjacentTo(const int32 OtherNodeIndex) const
	{
		for (const FLink Lk : Links) { if (Lk.Node == OtherNodeIndex) { return true; } }
		return false;
	}

	int32 FNode::GetEdgeIndex(const int32 AdjacentNodeIndex) const
	{
		for (const FLink Lk : Links) { if (Lk.Node == AdjacentNodeIndex) { return Lk.Edge; } }
		return -1;
	}
}

namespace PCGExClusters
{
	FNode::FNode(const int32 InNodeIndex, const int32 InPointIndex)
		: PCGExGraphs::FNode(InNodeIndex, InPointIndex)
	{
	}

	FVector FNode::GetCentroid(const FCluster* InCluster) const
	{
		if (Links.IsEmpty()) { return InCluster->GetPos(Index); }

		FVector Centroid = FVector::ZeroVector;
		const int32 NumPoints = Links.Num();

		for (int i = 0; i < NumPoints; i++) { Centroid += InCluster->GetPos(Links[i].Node); }

		if (Links.Num() < 2)
		{
			Centroid += InCluster->GetPos(Index);
			return Centroid / 2;
		}

		Centroid /= static_cast<double>(NumPoints);

		return Centroid;
	}

	int32 FNode::ValidEdges(const FCluster* InCluster)
	{
		int32 ValidNum = 0;
		for (const FLink Lk : Links) { if (InCluster->GetEdge(Lk.Edge)->bValid) { ValidNum++; } }
		return ValidNum;
	}

	bool FNode::HasAnyValidEdges(const FCluster* InCluster)
	{
		for (const FLink Lk : Links) { if (InCluster->GetEdge(Lk.Edge)->bValid) { return true; } }
		return false;
	}
}
