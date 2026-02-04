// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExEdgeRefineOperation.h"
#include "Clusters/PCGExCluster.h"
#include "PCGExEdgeRefineKeepShortest.generated.h"

class UPCGExHeuristicLocalDistance;
class FPCGExHeuristicDistance;

/**
 *
 */
class FPCGExEdgeKeepShortest : public FPCGExEdgeRefineOperation
{
public:
	virtual void ProcessNode(PCGExClusters::FNode& Node) override;
};

/**
 *
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Keep Shortest", PCGExNodeLibraryDoc="clusters/refine-cluster/edge-length"))
class UPCGExEdgeKeepShortest : public UPCGExEdgeRefineInstancedFactory
{
	GENERATED_BODY()

public:
	virtual bool GetDefaultEdgeValidity() const override { return false; }
	virtual bool WantsIndividualNodeProcessing() const override { return true; }

	PCGEX_CREATE_REFINE_OPERATION(EdgeKeepShortest, {})
};
