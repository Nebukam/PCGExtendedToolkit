// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Promoting/PCGExEdgePromoteToPoint.h"

#include "PCGExPointsProcessor.h"

void UPCGExEdgePromoteToPoint::PromoteEdge(const PCGExGraph::FUnsignedEdge& Edge, const FPCGPoint& StartPoint, const FPCGPoint& EndPoint)
{
	int32 Index;
	FPCGPoint& NewPoint = Context->CurrentIO->NewPoint(Index);
	NewPoint.Transform.SetLocation(FMath::Lerp(StartPoint.Transform.GetLocation(), EndPoint.Transform.GetLocation(), 0.5));
}
