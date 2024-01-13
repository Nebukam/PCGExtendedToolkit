// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Promoting/PCGExEdgePromotingOperation.h"

bool UPCGExEdgePromotingOperation::GeneratesNewPointData() { return false; }

void UPCGExEdgePromotingOperation::PromoteEdge(const PCGExGraph::FUnsignedEdge& Edge, const FPCGPoint& StartPoint, const FPCGPoint& EndPoint)
{
}

bool UPCGExEdgePromotingOperation::PromoteEdgeGen(UPCGPointData* InData, const PCGExGraph::FUnsignedEdge& Edge, const FPCGPoint& StartPoint, const FPCGPoint& EndPoint)
{
	return false;
}
