// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Relaxing/PCGExEdgeRelaxingOperation.h"

bool UPCGExEdgeRelaxingOperation::GeneratesNewPointData() { return false; }

void UPCGExEdgeRelaxingOperation::PromoteEdge(const PCGExGraph::FUnsignedEdge& Edge, const FPCGPoint& StartPoint, const FPCGPoint& EndPoint)
{
}

bool UPCGExEdgeRelaxingOperation::PromoteEdgeGen(UPCGPointData* InData, const PCGExGraph::FUnsignedEdge& Edge, const FPCGPoint& StartPoint, const FPCGPoint& EndPoint)
{
	return false;
}
