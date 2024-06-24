// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExPointData.h"

#include "PCGExClusterData.generated.h"

namespace PCGExData
{
	enum class EInit : uint8;
}

namespace PCGExCluster
{
	struct FCluster;
}

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExClusterData : public UPCGExPointData
{
	GENERATED_BODY()
};

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExClusterNodesData : public UPCGExClusterData
{
	GENERATED_BODY()

	mutable FRWLock BoundClustersLock;
	
public:
	TSet<PCGExCluster::FCluster*> BoundClusters;

	virtual void InitializeFromPCGExData(const UPCGExPointData* InPCGExPointData, const PCGExData::EInit InitMode) override;

	void AddBoundCluster(PCGExCluster::FCluster* InCluster);
	
	virtual void BeginDestroy() override;
	
protected:
	virtual UPCGSpatialData* CopyInternal() const override;
};

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExClusterEdgesData : public UPCGExClusterData
{
	GENERATED_BODY()

public:

	virtual void InitializeFromPCGExData(const UPCGExPointData* InPCGExPointData, const PCGExData::EInit InitMode) override;

	virtual void SetBoundCluster(PCGExCluster::FCluster* InCluster, bool bIsOwner);
	PCGExCluster::FCluster* GetBoundCluster() const;
	
	virtual void BeginDestroy() override;
	
protected:
	PCGExCluster::FCluster* Cluster = nullptr;
	virtual UPCGSpatialData* CopyInternal() const override;
	bool bOwnsCluster = true;
};
