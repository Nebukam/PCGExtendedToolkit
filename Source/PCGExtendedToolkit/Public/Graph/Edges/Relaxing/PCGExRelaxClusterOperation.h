// Copyright Timothé Lapetite 2024
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
	struct FCluster;
}

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExRelaxClusterOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual void PrepareForCluster(PCGExCluster::FCluster* InCluster);
	virtual void ProcessExpandedNode(const PCGExCluster::FExpandedNode* ExpandedNode);

	PCGExCluster::FCluster* Cluster = nullptr;
	TArray<PCGExCluster::FExpandedNode*>* ExpandedNodes = nullptr;
	TArray<FVector>* ReadBuffer = nullptr;
	TArray<FVector>* WriteBuffer = nullptr;

	virtual void Cleanup() override;
};
