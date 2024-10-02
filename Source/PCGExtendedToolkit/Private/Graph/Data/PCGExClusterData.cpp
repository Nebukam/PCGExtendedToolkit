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

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
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
#else
UPCGSpatialData* UPCGExClusterNodesData::CopyInternal(FPCGContext* Context) const
{
	PCGEX_ENFORCE_CONTEXT_ASYNC(Context)
	
	UPCGExClusterNodesData* NewNodeData = FPCGContext::NewObject_AnyThread<UPCGExClusterNodesData>(Context);
	NewNodeData->CopyFrom(this);
	return NewNodeData;
}
#endif


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

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
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
#else
UPCGSpatialData* UPCGExClusterEdgesData::CopyInternal(FPCGContext* Context) const
{
	PCGEX_ENFORCE_CONTEXT_ASYNC(Context)
	
	UPCGExClusterEdgesData* NewEdgeData = FPCGContext::NewObject_AnyThread<UPCGExClusterEdgesData>(Context);
	NewEdgeData->CopyFrom(this);
	return NewEdgeData;
}
#endif


void UPCGExClusterEdgesData::BeginDestroy()
{
	Super::BeginDestroy();
	Cluster.Reset();
}
