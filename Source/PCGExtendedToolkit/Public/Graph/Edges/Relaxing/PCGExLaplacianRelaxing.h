// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRelaxingOperation.h"
#include "PCGExLaplacianRelaxing.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Laplacian (Poisson)")
class PCGEXTENDEDTOOLKIT_API UPCGExLaplacianRelaxing : public UPCGExEdgeRelaxingOperation
{
	GENERATED_BODY()

public:
	virtual void ProcessVertex(const PCGExCluster::FNode& Vertex) override;
};
