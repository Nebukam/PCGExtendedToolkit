// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExRelaxClusterOperation.h"
#include "PCGExLaplacianRelax.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Laplacian (Poisson)")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExLaplacianRelax : public UPCGExRelaxClusterOperation
{
	GENERATED_BODY()

public:
	virtual void Step1(const PCGExCluster::FNode& Node) override
	{
		const FVector Position = (ReadBuffer->GetData() + Node.Index)->GetLocation();
		FVector Force = FVector::Zero();

		for (const PCGExGraph::FLink& Lk : Node.Links)
		{
			Force += (ReadBuffer->GetData() + Lk.Node)->GetLocation() - Position;
		}

		(*WriteBuffer)[Node.Index].SetLocation(Position + Force / static_cast<double>(Node.Links.Num()));
	}
};
