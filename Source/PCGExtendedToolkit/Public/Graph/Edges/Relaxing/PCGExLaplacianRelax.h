// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExRelaxClusterOperation.h"
#include "PCGExLaplacianRelax.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, meta=(DisplayName = "Laplacian (Poisson)", PCGExNodeLibraryDoc="clusters/relax-cluster/laplacian-poisson"))
class UPCGExLaplacianRelax : public UPCGExRelaxClusterOperation
{
	GENERATED_BODY()

public:
	virtual void Step1(const PCGExClusters::FNode& Node) override
	{
		const FVector Position = (ReadBuffer->GetData() + Node.Index)->GetLocation();
		FVector Force = FVector::ZeroVector;

		for (const PCGExGraphs::FLink& Lk : Node.Links) { Force += (ReadBuffer->GetData() + Lk.Node)->GetLocation() - Position; }

		(*WriteBuffer)[Node.Index].SetLocation(Position + Force / static_cast<double>(Node.Links.Num()));
	}
};
