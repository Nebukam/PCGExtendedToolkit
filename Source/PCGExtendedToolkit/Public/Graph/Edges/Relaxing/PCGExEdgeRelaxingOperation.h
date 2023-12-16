// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOperation.h"
#include "Graph/PCGExGraph.h"
#include "PCGExEdgeRelaxingOperation.generated.h"

/**
 * 
 */
UCLASS(Abstract, Blueprintable, EditInlineNew)
class PCGEXTENDEDTOOLKIT_API UPCGExEdgeRelaxingOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual bool GeneratesNewPointData();
	virtual void PromoteEdge(const PCGExGraph::FUnsignedEdge& Edge, const FPCGPoint& StartPoint, const FPCGPoint& EndPoint);
	virtual bool PromoteEdgeGen(UPCGPointData* InData, const PCGExGraph::FUnsignedEdge& Edge, const FPCGPoint& StartPoint, const FPCGPoint& EndPoint);
};
