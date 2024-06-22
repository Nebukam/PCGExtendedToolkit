// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "PCGExVtxExtraFactoryProvider.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExGraph.h"

#include "PCGExVtxExtraSpecialNeighbors.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSpecialNeighborsSettings
{
	GENERATED_BODY()

	/** Shortest edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExEdgeOutputWithIndexSettings LargestNeighbor = FPCGExEdgeOutputWithIndexSettings(TEXT("Largest"));

	/** Longest edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExEdgeOutputWithIndexSettings SmallestNeighbor = FPCGExEdgeOutputWithIndexSettings(TEXT("Smallest"));
};

/**
 * ó
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExVtxExtraSpecialNeighbors : public UPCGExVtxExtraOperation
{
	GENERATED_BODY()

public:
	FPCGExSpecialNeighborsSettings Descriptor;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;
	virtual bool PrepareForVtx(const FPCGContext* InContext, PCGExData::FPointIO* InVtx) override;
	virtual void ProcessNode(const int32 ClusterIdx, const PCGExCluster::FCluster* Cluster, PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency) override;
	virtual void Write() override;
	virtual void Write(PCGExMT::FTaskManager* AsyncManager) override;
	virtual void Cleanup() override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExVtxExtraSpecialNeighborsFactory : public UPCGExVtxExtraFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExSpecialNeighborsSettings Descriptor;
	virtual UPCGExVtxExtraOperation* CreateOperation() const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|VtxExtra")
class PCGEXTENDEDTOOLKIT_API UPCGExVtxExtraSpecialNeighborsSettings : public UPCGExVtxExtraProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(NeighborSamplerAttribute, "Vtx Extra : Special Neighbors", "Fetch data from neighbors")

#endif
	//~End UPCGSettings

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Direction Settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExSpecialNeighborsSettings Descriptor;
};
