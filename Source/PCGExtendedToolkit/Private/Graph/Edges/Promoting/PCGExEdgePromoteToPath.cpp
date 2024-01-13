// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Promoting/PCGExEdgePromoteToPath.h"

bool UPCGExEdgePromoteToPath::GeneratesNewPointData() { return true; }

bool UPCGExEdgePromoteToPath::PromoteEdgeGen(UPCGPointData* InData, const PCGExGraph::FUnsignedEdge& Edge, const FPCGPoint& StartPoint, const FPCGPoint& EndPoint)
{
	TArray<FPCGPoint>& MutablePoints = InData->GetMutablePoints();
	MutablePoints.Emplace_GetRef(StartPoint);
	MutablePoints.Emplace_GetRef(EndPoint);
	return true;
}
