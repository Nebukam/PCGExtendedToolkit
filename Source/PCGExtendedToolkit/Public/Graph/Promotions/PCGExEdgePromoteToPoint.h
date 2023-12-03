// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgePromotion.h"
#include "PCGExEdgePromoteToPoint.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew, DisplayName = "Promote To Points")
class PCGEXTENDEDTOOLKIT_API UPCGExEdgePromoteToPoint : public UPCGExEdgePromotion
{
	GENERATED_BODY()
public:
	virtual void PromoteEdge(const PCGExGraph::FUnsignedEdge& Edge, const FPCGPoint& StartPoint, const FPCGPoint& EndPoint) override;
};
