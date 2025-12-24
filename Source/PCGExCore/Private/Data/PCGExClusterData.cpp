// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/PCGExClusterData.h"

#include "PCGExCoreSettingsCache.h"
#include "Data/PCGExPointIO.h"

#include "PCGExSettingsCacheBody.h"
#include "Clusters/PCGExCluster.h"

PCG_DEFINE_TYPE_INFO(FPCGExDataTypeInfoClusterPart, UPCGExClusterData)
PCG_DEFINE_TYPE_INFO(FPCGExDataTypeInfoVtx, UPCGExClusterNodesData)
PCG_DEFINE_TYPE_INFO(FPCGExDataTypeInfoEdges, UPCGExClusterEdgesData)

#if WITH_EDITOR
bool FPCGExDataTypeInfoClusterPart::Hidden() const
{
	return true;
}
#endif // WITH_EDITOR

UPCGSpatialData* UPCGExClusterNodesData::CopyInternal(FPCGContext* Context) const
{
	PCGEX_NEW_CUSTOM_POINT_DATA(UPCGExClusterNodesData)
	return NewData;
}

void UPCGExClusterEdgesData::InitializeSpatialDataInternal(const FPCGInitializeFromDataParams& InParams)
{
	Super::InitializeSpatialDataInternal(InParams);
	if (const UPCGExClusterEdgesData* InEdgeData = Cast<UPCGExClusterEdgesData>(InParams.Source); InEdgeData && PCGEX_CORE_SETTINGS.bCacheClusters)
	{
		SetBoundCluster(InEdgeData->Cluster);
	}
}

UPCGSpatialData* UPCGExClusterEdgesData::CopyInternal(FPCGContext* Context) const
{
	PCGEX_NEW_CUSTOM_POINT_DATA(UPCGExClusterEdgesData)
	return NewData;
}

void UPCGExClusterEdgesData::SetBoundCluster(const TSharedPtr<PCGExClusters::FCluster>& InCluster)
{
	Cluster = InCluster;
}

const TSharedPtr<PCGExClusters::FCluster>& UPCGExClusterEdgesData::GetBoundCluster() const
{
	return Cluster;
}

void UPCGExClusterEdgesData::BeginDestroy()
{
	Super::BeginDestroy();
	Cluster.Reset();
}
