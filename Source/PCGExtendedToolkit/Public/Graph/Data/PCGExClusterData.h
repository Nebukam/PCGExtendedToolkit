// Copyright 2024 Timothé Lapetite and contributors
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
	enum class EIOInit : uint8;
}

namespace PCGExCluster
{
	class FCluster;
}

/**
 * 
 */
UCLASS(Abstract)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExClusterData : public UPCGExPointData
{
	GENERATED_BODY()
};

/**
 * 
 */
UCLASS()
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExClusterNodesData : public UPCGExClusterData
{
	GENERATED_BODY()

	mutable FRWLock BoundClustersLock;

public:
	virtual void InitializeFromPCGExData(const UPCGExPointData* InPCGExPointData, const PCGExData::EIOInit InitMode) override;

	virtual void BeginDestroy() override;

protected:
	//TSharedPtr<TMap<uint32, int32>> EndpointsLookup;
	virtual void DoEarlyCleanup() const override;

#if PCGEX_ENGINE_VERSION < 505
	virtual UPCGSpatialData* CopyInternal() const override;
#else
	virtual UPCGSpatialData* CopyInternal(FPCGContext* Context) const override;
#endif
};

/**
 * 
 */
UCLASS()
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExClusterEdgesData : public UPCGExClusterData
{
	GENERATED_BODY()

public:
	virtual void InitializeFromPCGExData(const UPCGExPointData* InPCGExPointData, const PCGExData::EIOInit InitMode) override;

	virtual void SetBoundCluster(const TSharedPtr<PCGExCluster::FCluster>& InCluster);
	const TSharedPtr<PCGExCluster::FCluster>& GetBoundCluster() const;

	virtual void BeginDestroy() override;

protected:
	TSharedPtr<PCGExCluster::FCluster> Cluster;

	virtual void DoEarlyCleanup() const override;

#if PCGEX_ENGINE_VERSION < 505
	virtual UPCGSpatialData* CopyInternal() const override;
#else
	virtual UPCGSpatialData* CopyInternal(FPCGContext* Context) const override;
#endif
};

namespace PCGExClusterData
{
	static TSharedPtr<PCGExCluster::FCluster> TryGetCachedCluster(const TSharedRef<PCGExData::FPointIO>& VtxIO, const TSharedRef<PCGExData::FPointIO>& EdgeIO)
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
}
