// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "PCGExEdgeRefineRemoveShortest.generated.h"

class UPCGExHeuristicLocalDistance;
class UPCGExHeuristicDistance;

namespace PCGExCluster
{
	struct FNode;
}

/**
 * 
 */
UCLASS(BlueprintType, meta=(DisplayName="Remove Shortest"))
class PCGEXTENDEDTOOLKIT_API UPCGExEdgeRemoveShortest : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual bool RequiresIndividualNodeProcessing() override { return true; }
	virtual void ProcessNode(PCGExCluster::FNode& Node) override;
};
