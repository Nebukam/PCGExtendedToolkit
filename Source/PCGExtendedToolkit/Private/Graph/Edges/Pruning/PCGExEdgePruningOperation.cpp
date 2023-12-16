// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Pruning/PCGExEdgePruningOperation.h"

bool UPCGExEdgePruningOperation::GeneratesNewPointData() { return false; }

void UPCGExEdgePruningOperation::PromoteEdge(const PCGExGraph::FUnsignedEdge& Edge, const FPCGPoint& StartPoint, const FPCGPoint& EndPoint)
{
}

bool UPCGExEdgePruningOperation::PromoteEdgeGen(UPCGPointData* InData, const PCGExGraph::FUnsignedEdge& Edge, const FPCGPoint& StartPoint, const FPCGPoint& EndPoint)
{
	return false;
}
