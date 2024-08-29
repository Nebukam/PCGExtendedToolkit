// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "PCGExEdgeRefineKeepLongest.generated.h"

class UPCGExHeuristicLocalDistance;
class UPCGExHeuristicDistance;

namespace PCGExCluster
{
	struct FNode;
}

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Keep Longest"))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeKeepLongest : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual bool InvalidateAllEdgesBeforeProcessing() override { return true; }
	virtual bool RequiresIndividualNodeProcessing() override { return true; }
	virtual void ProcessNode(PCGExCluster::FNode& Node) override;
};
