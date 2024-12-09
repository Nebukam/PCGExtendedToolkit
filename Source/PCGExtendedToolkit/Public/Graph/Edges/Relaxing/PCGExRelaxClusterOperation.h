// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOperation.h"
#include "Graph/PCGExCluster.h"
#include "PCGExRelaxClusterOperation.generated.h"

namespace PCGExCluster
{
	struct FNode;
}

namespace PCGExCluster
{
	class FCluster;
}

/**
 * 
 */
UCLASS(Abstract)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExRelaxClusterOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override
	{
		Super::CopySettingsFrom(Other);
		//if (const UPCGExRelaxClusterOperation* TypedOther = Cast<UPCGExRelaxClusterOperation>(Other))		{		}
	}

	virtual void PrepareForCluster(PCGExCluster::FCluster* InCluster)
	{
		Cluster = InCluster;
	}

	virtual void ProcessExpandedNode(const PCGExCluster::FNode& Node)
	{
	}

	PCGExCluster::FCluster* Cluster = nullptr;
	TArray<FVector>* ReadBuffer = nullptr;
	TArray<FVector>* WriteBuffer = nullptr;

	virtual void Cleanup() override
	{
		Cluster = nullptr;
		ReadBuffer = nullptr;
		WriteBuffer = nullptr;

		Super::Cleanup();
	}
};
