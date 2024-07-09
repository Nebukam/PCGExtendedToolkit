// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "PCGExEdgeRefineKeepShortest.generated.h"

class UPCGExHeuristicLocalDistance;
class UPCGExHeuristicDistance;

namespace PCGExCluster
{
	struct FNode;
}

/**
 * 
 */
UCLASS(BlueprintType, meta=(DisplayName="Keep Shortest"))
class PCGEXTENDEDTOOLKIT_API UPCGExEdgeKeepShortest : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual bool InvalidateAllEdgesBeforeProcessing() override { return true; }
	virtual bool RequiresIndividualNodeProcessing() override { return true; }
	virtual void ProcessNode(PCGExCluster::FNode& Node) override;
};
