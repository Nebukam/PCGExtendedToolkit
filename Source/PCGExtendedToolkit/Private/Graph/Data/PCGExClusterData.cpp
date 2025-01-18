// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Data/PCGExClusterData.h"

#include "Data/PCGExPointIO.h"
#include "PCGExGlobalSettings.h"
#include "Graph/PCGExCluster.h"

void UPCGExClusterNodesData::InitializeFromPCGExData(const UPCGExPointData* InPCGExPointData, const PCGExData::EIOInit InitMode)
{
	Super::InitializeFromPCGExData(InPCGExPointData, InitMode);
	// if (const UPCGExClusterNodesData* InNodeData = Cast<UPCGExClusterNodesData>(InPCGExPointData))	{	}
}

void UPCGExClusterNodesData::BeginDestroy()
{
	Super::BeginDestroy();
}

//PCGEX_DATA_COPY_INTERNAL_IMPL(UPCGExClusterNodesData)

void UPCGExClusterEdgesData::InitializeFromPCGExData(const UPCGExPointData* InPCGExPointData, const PCGExData::EIOInit InitMode)
{
	Super::InitializeFromPCGExData(InPCGExPointData, InitMode);
	if (const UPCGExClusterEdgesData* InEdgeData = Cast<UPCGExClusterEdgesData>(InPCGExPointData))
	{
		if (GetDefault<UPCGExGlobalSettings>()->bCacheClusters)
		{
			if (InitMode != PCGExData::EIOInit::None &&
				InitMode != PCGExData::EIOInit::New)
			{
				SetBoundCluster(InEdgeData->Cluster);
			}
		}
	}
}

void UPCGExClusterEdgesData::SetBoundCluster(const TSharedPtr<PCGExCluster::FCluster>& InCluster)
{
	Cluster = InCluster;
}

const TSharedPtr<PCGExCluster::FCluster>& UPCGExClusterEdgesData::GetBoundCluster() const
{
	return Cluster;
}

//PCGEX_DATA_COPY_INTERNAL_IMPL(UPCGExClusterEdgesData)

void UPCGExClusterEdgesData::BeginDestroy()
{
	Super::BeginDestroy();
	Cluster.Reset();
}
