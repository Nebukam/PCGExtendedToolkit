// Fill out your copyright notice in the Description page of Project Settings.


#include "Graph/PCGExClusterData.h"

#include "Data/PCGExPointIO.h"

void UPCGExClusterNodesData::InitializeFromPCGExData(const UPCGExPointData* InPCGExPointData, const PCGExData::EInit InitMode)
{
	Super::InitializeFromPCGExData(InPCGExPointData, InitMode);
	const UPCGExClusterNodesData* InNodeData = Cast<UPCGExClusterNodesData>(InPCGExPointData);
	if (InNodeData)
	{
	}
}

UPCGSpatialData* UPCGExClusterNodesData::CopyInternal() const
{
	UPCGExClusterNodesData* NewNodeData = NewObject<UPCGExClusterNodesData>();
	NewNodeData->CopyFrom(this);
	return NewNodeData;
}

void UPCGExClusterEdgesData::InitializeFromPCGExData(const UPCGExPointData* InPCGExPointData, const PCGExData::EInit InitMode)
{
	Super::InitializeFromPCGExData(InPCGExPointData, InitMode);
	const UPCGExClusterEdgesData* InEdgeData = Cast<UPCGExClusterEdgesData>(InPCGExPointData);
	if (InEdgeData)
	{
	}
}

UPCGSpatialData* UPCGExClusterEdgesData::CopyInternal() const
{
	UPCGExClusterEdgesData* NewEdgeData = NewObject<UPCGExClusterEdgesData>();
	NewEdgeData->CopyFrom(this);
	return NewEdgeData;
}
