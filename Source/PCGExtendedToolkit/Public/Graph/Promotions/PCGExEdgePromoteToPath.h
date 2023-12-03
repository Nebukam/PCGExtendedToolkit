// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgePromotion.h"
#include "PCGExEdgePromoteToPath.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew, DisplayName = "Promote To Paths")
class PCGEXTENDEDTOOLKIT_API UPCGExEdgePromoteToPath : public UPCGExEdgePromotion
{
	GENERATED_BODY()

public:
	virtual bool GeneratesNewPointData() override;
	virtual bool PromoteEdgeGen(UPCGPointData* InData, const PCGExGraph::FUnsignedEdge& Edge, const FPCGPoint& StartPoint, const FPCGPoint& EndPoint) override;
};
