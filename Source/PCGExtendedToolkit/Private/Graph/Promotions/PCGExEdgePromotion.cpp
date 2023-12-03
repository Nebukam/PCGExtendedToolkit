// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Promotions/PCGExEdgePromotion.h"

bool UPCGExEdgePromotion::GeneratesNewPointData() { return false; }

void UPCGExEdgePromotion::PromoteEdge(const PCGExGraph::FUnsignedEdge& Edge, const FPCGPoint& StartPoint, const FPCGPoint& EndPoint)
{
}

bool UPCGExEdgePromotion::PromoteEdgeGen(UPCGPointData* InData, const PCGExGraph::FUnsignedEdge& Edge, const FPCGPoint& StartPoint, const FPCGPoint& EndPoint)
{
	return false;
}
