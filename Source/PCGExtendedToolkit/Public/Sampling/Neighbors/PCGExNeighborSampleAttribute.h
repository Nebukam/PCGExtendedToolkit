// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExGraph.h"

#include "PCGExNeighborSampleFactoryProvider.h"
#include "Data/Blending/PCGExMetadataBlender.h"


#include "PCGExNeighborSampleAttribute.generated.h"

///

/**
 * 
 */
class FPCGExNeighborSampleAttribute : public FPCGExNeighborSampleOperation
{
public:
	TSharedPtr<PCGExDataBlending::FMetadataBlender> Blender;

	FPCGExAttributeSourceToTargetList SourceAttributes;
	EPCGExDataBlendingType Blending = EPCGExDataBlendingType::Average;

	virtual void PrepareForCluster(FPCGExContext* InContext, TSharedRef<PCGExCluster::FCluster> InCluster, TSharedRef<PCGExData::FFacade> InVtxDataFacade, TSharedRef<PCGExData::FFacade> InEdgeDataFacade) override;

	virtual void PrepareNode(const PCGExCluster::FNode& TargetNode) const override;
	virtual void SampleNeighborNode(const PCGExCluster::FNode& TargetNode, const PCGExGraph::FLink Lk, const double Weight) override;
	virtual void SampleNeighborEdge(const PCGExCluster::FNode& TargetNode, const PCGExGraph::FLink Lk, const double Weight) override;
	virtual void FinalizeNode(const PCGExCluster::FNode& TargetNode, const int32 Count, const double TotalWeight) override;
	virtual void CompleteOperation() override;

protected:
	FPCGExBlendingDetails MetadataBlendingDetails;
};


USTRUCT(BlueprintType)
struct FPCGExAttributeSamplerConfigBase
{
	GENERATED_BODY()

	FPCGExAttributeSamplerConfigBase()
	{
	}

	/** Unique blendmode applied to all specified attributes. For different blendmodes, create multiple sampler nodes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDataBlendingType Blending = EPCGExDataBlendingType::Average;

	/** Attribute to sample & optionally remap. Leave it to None to overwrite the source attribute.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExAttributeSourceToTargetList SourceAttributes;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExNeighborSamplerFactoryAttribute : public UPCGExNeighborSamplerFactoryData
{
	GENERATED_BODY()

public:
	FPCGExAttributeSamplerConfigBase Config;
	virtual TSharedPtr<FPCGExNeighborSampleOperation> CreateOperation(FPCGExContext* InContext) const override;

	virtual bool RegisterConsumableAttributes(FPCGExContext* InContext) const override;
	virtual void RegisterVtxBuffersDependencies(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, PCGExData::FFacadePreloader& FacadePreloader) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|NeighborSample")
class UPCGExNeighborSampleAttributeSettings : public UPCGExNeighborSampleProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NeighborSamplerAttribute, "Sampler : Vtx Attributes", "Create a single neighbor attribute sampler, to be used by a Sample Neighbors node.",
		PCGEX_FACTORY_NAME_PRIORITY)

#endif
	//~End UPCGSettings

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Sampler Settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExAttributeSamplerConfigBase Config;
};
