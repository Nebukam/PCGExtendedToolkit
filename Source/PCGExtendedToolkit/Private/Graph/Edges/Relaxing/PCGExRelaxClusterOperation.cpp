// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Relaxing/PCGExRelaxClusterOperation.h"

void UPCGExRelaxClusterOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExRelaxClusterOperation* TypedOther = Cast<UPCGExRelaxClusterOperation>(Other))
	{
	}
}

void UPCGExRelaxClusterOperation::PrepareForCluster(PCGExCluster::FCluster* InCluster)
{
	Cluster = InCluster;
}

void UPCGExRelaxClusterOperation::ProcessExpandedNode(const PCGExCluster::FExpandedNode* ExpandedNode)
{
}

void UPCGExRelaxClusterOperation::Cleanup()
{
	Cluster = nullptr;
	ReadBuffer = nullptr;
	WriteBuffer = nullptr;

	Super::Cleanup();
}
