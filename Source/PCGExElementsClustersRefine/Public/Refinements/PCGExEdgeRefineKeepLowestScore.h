// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExEdgeRefineOperation.h"
#include "Clusters/PCGExCluster.h"
#include "PCGExEdgeRefineKeepLowestScore.generated.h"

class UPCGExHeuristicLocalDistance;
class FPCGExHeuristicDistance;

/**
 *
 */
class FPCGExEdgeKeepLowestScore : public FPCGExEdgeRefineOperation
{
public:
	virtual void ProcessNode(PCGExClusters::FNode& Node) override;
};

/**
 *
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Keep Lowest Score", PCGExNodeLibraryDoc="clusters/refine-cluster/edge-score"))
class UPCGExEdgeKeepLowestScore : public UPCGExEdgeRefineInstancedFactory
{
	GENERATED_BODY()

public:
	virtual bool GetDefaultEdgeValidity() const override { return false; }
	virtual bool WantsHeuristics() const override { return true; }
	virtual bool WantsIndividualNodeProcessing() const override { return true; }

	PCGEX_CREATE_REFINE_OPERATION(EdgeKeepLowestScore, {})
};
