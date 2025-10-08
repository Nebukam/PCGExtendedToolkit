﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFactoryProvider.h"
#include "Data/PCGExPointData.h"

#include "PCGExClusterData.generated.h"

namespace PCGExData
{
	class FPointIO;
	enum class EIOInit : uint8;
}

namespace PCGExCluster
{
	class FCluster;
}

USTRUCT(/*PCG_DataType*/DisplayName="PCGEx | Cluster Part")
struct FPCGExDataTypeInfoClusterPart : public FPCGDataTypeInfoPoint
{
	GENERATED_BODY()
	PCG_DECLARE_TYPE_INFO(PCGEXTENDEDTOOLKIT_API)

#if WITH_EDITOR
	virtual bool Hidden() const override;
#endif // WITH_EDITOR
};

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExClusterData : public UPCGExPointData
{
	GENERATED_BODY()

public:
	PCG_ASSIGN_TYPE_INFO(FPCGExDataTypeInfoClusterPart)
};

USTRUCT(/*PCG_DataType*/DisplayName="PCGEx | Cluster Vtx")
struct FPCGExDataTypeInfoVtx : public FPCGExDataTypeInfoClusterPart
{
	GENERATED_BODY()
	PCG_DECLARE_TYPE_INFO(PCGEXTENDEDTOOLKIT_API)
};

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExClusterNodesData : public UPCGExClusterData
{
	GENERATED_BODY()

	mutable FRWLock BoundClustersLock;

protected:
	PCG_ASSIGN_TYPE_INFO(FPCGExDataTypeInfoVtx)

	virtual UPCGSpatialData* CopyInternal(FPCGContext* Context) const override;
};

USTRUCT(/*PCG_DataType*/DisplayName="PCGEx | Cluster Edges")
struct FPCGExDataTypeInfoEdges : public FPCGExDataTypeInfoClusterPart
{
	GENERATED_BODY()
	PCG_DECLARE_TYPE_INFO(PCGEXTENDEDTOOLKIT_API)
};

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExClusterEdgesData : public UPCGExClusterData
{
	GENERATED_BODY()

public:
	PCG_ASSIGN_TYPE_INFO(FPCGExDataTypeInfoEdges)

	virtual void InitializeSpatialDataInternal(const FPCGInitializeFromDataParams& InParams) override;

	virtual void SetBoundCluster(const TSharedPtr<PCGExCluster::FCluster>& InCluster);
	const TSharedPtr<PCGExCluster::FCluster>& GetBoundCluster() const;

	virtual void BeginDestroy() override;

protected:
	TSharedPtr<PCGExCluster::FCluster> Cluster;

	virtual UPCGSpatialData* CopyInternal(FPCGContext* Context) const override;
};

namespace PCGExClusterData
{
	TSharedPtr<PCGExCluster::FCluster> TryGetCachedCluster(const TSharedRef<PCGExData::FPointIO>& VtxIO, const TSharedRef<PCGExData::FPointIO>& EdgeIO);
}
