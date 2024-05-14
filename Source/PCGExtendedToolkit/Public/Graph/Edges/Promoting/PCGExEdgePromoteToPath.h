// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgePromotingOperation.h"
#include "PCGExEdgePromoteToPath.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, meta=(DisplayName="Edges To Paths"))
class PCGEXTENDEDTOOLKIT_API UPCGExEdgePromoteToPath : public UPCGExEdgePromotingOperation
{
	GENERATED_BODY()

public:
	virtual bool GeneratesNewPointData() override;
	virtual bool PromoteEdgeGen(UPCGPointData* InData, const PCGExGraph::FUnsignedEdge& Edge, const FPCGPoint& StartPoint, const FPCGPoint& EndPoint) override;
};
