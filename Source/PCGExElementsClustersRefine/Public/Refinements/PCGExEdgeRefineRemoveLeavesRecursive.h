// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExEdgeRefineOperation.h"
#include "Clusters/PCGExCluster.h"
#include "Async/ParallelFor.h"

#include "PCGExEdgeRefineRemoveLeavesRecursive.generated.h"

/**
 *
 */
class FPCGExEdgeRemoveLeavesRecursive : public FPCGExEdgeRefineOperation
{
public:
	virtual void Process() override;

	int32 MaxIterations = 0;
};

/**
 *
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Remove Leaves (Recursive)", PCGExNodeLibraryDoc="clusters/refine-cluster/remove-leaves-recursive"))
class UPCGExEdgeRemoveLeavesRecursive : public UPCGExEdgeRefineInstancedFactory
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override;

	/** Maximum number of iterations. Set to 0 or less for unlimited iterations until no leaves remain. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 MaxIterations = 0;

	PCGEX_CREATE_REFINE_OPERATION(EdgeRemoveLeavesRecursive, { Operation->MaxIterations = MaxIterations; })
};
