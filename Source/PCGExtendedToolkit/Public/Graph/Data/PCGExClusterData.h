// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"
#include "Data/PCGExPointData.h"
#include "Data/PCGExPointIO.h"
#include "Graph/PCGExCluster.h"

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

namespace PCGExClusterData
{
	static const PCGExCluster::FCluster* TryGetCachedCluster(PCGExData::FPointIO* VtxIO, PCGExData::FPointIO* EdgeIO)
	{
		if (GetDefault<UPCGExGlobalSettings>()->bCacheClusters)
		{
			if (const UPCGExClusterEdgesData* ClusterEdgesData = Cast<UPCGExClusterEdgesData>(EdgeIO->GetIn()))
			{
				//Try to fetch cached cluster
				if (const PCGExCluster::FCluster* CachedCluster = ClusterEdgesData->GetBoundCluster())
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
}
