// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Clusters/PCGExEdge.h"
#include "Clusters/PCGExCluster.h"

namespace PCGExGraphs
{
}

namespace PCGExClusters
{
	FBoundedEdge::FBoundedEdge(const FCluster* Cluster, const int32 InEdgeIndex)
		: Index(InEdgeIndex), Bounds(FBoxSphereBounds(FSphere(FMath::Lerp(Cluster->GetStartPos(InEdgeIndex), Cluster->GetEndPos(InEdgeIndex), 0.5), Cluster->GetDist(InEdgeIndex) * 0.5)))
	{
	}

	FBoundedEdge::FBoundedEdge()
		: Index(-1), Bounds(FBoxSphereBounds(ForceInit))
	{
	}
}
