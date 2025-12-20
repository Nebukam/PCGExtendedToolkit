// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/PCGExClusterData.h"

#include "Data/PCGExPointIO.h"
#include "PCGExGlobalSettings.h"
#include "Graph/PCGExCluster.h"

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
	if (const UPCGExClusterEdgesData* InEdgeData = Cast<UPCGExClusterEdgesData>(InParams.Source); InEdgeData && GetDefault<UPCGExGlobalSettings>()->bCacheClusters)
	{
		SetBoundCluster(InEdgeData->Cluster);
	}
}

UPCGSpatialData* UPCGExClusterEdgesData::CopyInternal(FPCGContext* Context) const
{
	PCGEX_NEW_CUSTOM_POINT_DATA(UPCGExClusterEdgesData)
	return NewData;
}

void UPCGExClusterEdgesData::SetBoundCluster(const TSharedPtr<PCGExCluster::FCluster>& InCluster)
{
	Cluster = InCluster;
}

const TSharedPtr<PCGExCluster::FCluster>& UPCGExClusterEdgesData::GetBoundCluster() const
{
	return Cluster;
}

void UPCGExClusterEdgesData::BeginDestroy()
{
	Super::BeginDestroy();
	Cluster.Reset();
}

TSharedPtr<PCGExCluster::FCluster> PCGExClusterData::TryGetCachedCluster(const TSharedRef<PCGExData::FPointIO>& VtxIO, const TSharedRef<PCGExData::FPointIO>& EdgeIO)
{
	if (GetDefault<UPCGExGlobalSettings>()->bCacheClusters)
	{
		if (const UPCGExClusterEdgesData* ClusterEdgesData = Cast<UPCGExClusterEdgesData>(EdgeIO->GetIn()))
		{
			//Try to fetch cached cluster
			if (const TSharedPtr<PCGExCluster::FCluster>& CachedCluster = ClusterEdgesData->GetBoundCluster())
			{
				// Cheap validation -- if there are artifact use SanitizeCluster node, it's still incredibly cheaper.
				if (CachedCluster->IsValidWith(VtxIO, EdgeIO))
				{
					return CachedCluster;
				}
			}
		}
	}

	return nullptr;
}
