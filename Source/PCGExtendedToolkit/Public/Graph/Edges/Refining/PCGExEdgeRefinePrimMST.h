// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "PCGExEdgeRefinePrimMST.generated.h"

class UPCGExHeuristicLocalDistance;
class UPCGExHeuristicDistance;

namespace PCGExCluster
{
	struct FNode;
}

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="MST (Prim)"))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeRefinePrimMST : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual bool SupportFilters() override { return false; }
	virtual bool InvalidateAllEdgesBeforeProcessing() override { return true; }
	virtual bool RequiresHeuristics() override { return true; }
	virtual void Process() override;
};
