// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Data/PCGExClusterData.h"

#include "Data/PCGExPointIO.h"
#include "PCGExGlobalSettings.h"
#include "Graph/PCGExCluster.h"

void UPCGExClusterNodesData::InitializeFromPCGExData(const UPCGExPointData* InPCGExPointData, const PCGExData::EInit InitMode)
{
	Super::InitializeFromPCGExData(InPCGExPointData, InitMode);
	if (const UPCGExClusterNodesData* InNodeData = Cast<UPCGExClusterNodesData>(InPCGExPointData))
	{
	}
}

void UPCGExClusterNodesData::AddBoundCluster(PCGExCluster::FCluster* InCluster)
{
	// Vtx cannot own bound clusters
	FWriteScopeLock WriteScopeLock(BoundClustersLock);
	BoundClusters.Add(InCluster);
}

void UPCGExClusterNodesData::BeginDestroy()
{
	Super::BeginDestroy();
	BoundClusters.Empty();
}

UPCGSpatialData* UPCGExClusterNodesData::CopyInternal() const
{
	UPCGExClusterNodesData* NewNodeData = nullptr;
	{
		FGCScopeGuard GCGuard;
		NewNodeData = NewObject<UPCGExClusterNodesData>();
	}
	NewNodeData->CopyFrom(this);
	return NewNodeData;
}

void UPCGExClusterEdgesData::InitializeFromPCGExData(const UPCGExPointData* InPCGExPointData, const PCGExData::EInit InitMode)
{
	Super::InitializeFromPCGExData(InPCGExPointData, InitMode);
	if (const UPCGExClusterEdgesData* InEdgeData = Cast<UPCGExClusterEdgesData>(InPCGExPointData))
	{
		if (GetDefault<UPCGExGlobalSettings>()->bCacheClusters)
		{
			if (InitMode != PCGExData::EInit::NoOutput &&
				InitMode != PCGExData::EInit::NewOutput)
			{
				SetBoundCluster(InEdgeData->Cluster, false);
			}
		}
	}
}

void UPCGExClusterEdgesData::SetBoundCluster(PCGExCluster::FCluster* InCluster, const bool bIsOwner)
{
	Cluster = InCluster;
	bOwnsCluster = bIsOwner;
}

PCGExCluster::FCluster* UPCGExClusterEdgesData::GetBoundCluster() const
{
	return Cluster;
}

UPCGSpatialData* UPCGExClusterEdgesData::CopyInternal() const
{
	UPCGExClusterEdgesData* NewEdgeData = nullptr;
	{
		FGCScopeGuard GCGuard;
		NewEdgeData = NewObject<UPCGExClusterEdgesData>();
	}
	NewEdgeData->CopyFrom(this);
	return NewEdgeData;
}

void UPCGExClusterEdgesData::BeginDestroy()
{
	Super::BeginDestroy();
	if (Cluster && bOwnsCluster) { PCGEX_DELETE(Cluster) }
}
