// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExRelaxClusterOperation.h"
#include "PCGExLaplacianRelax.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Laplacian (Poisson)")
class PCGEXTENDEDTOOLKIT_API UPCGExLaplacianRelax : public UPCGExRelaxClusterOperation
{
	GENERATED_BODY()

public:
	virtual void ProcessExpandedNode(const PCGExCluster::FExpandedNode* ExpandedNode) override;
};
