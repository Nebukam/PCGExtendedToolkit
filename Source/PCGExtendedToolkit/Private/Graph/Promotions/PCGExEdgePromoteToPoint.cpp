// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Promotions/PCGExEdgePromoteToPoint.h"

#include "PCGExPointsProcessor.h"

void UPCGExEdgePromoteToPoint::PromoteEdge(const PCGExGraph::FUnsignedEdge& Edge, const FPCGPoint& StartPoint, const FPCGPoint& EndPoint)
{
	FPCGPoint& NewPoint = Context->CurrentIO->NewPoint();
	NewPoint.Transform.SetLocation(FMath::Lerp(StartPoint.Transform.GetLocation(), EndPoint.Transform.GetLocation(), 0.5));
}
