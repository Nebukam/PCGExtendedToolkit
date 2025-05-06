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
#include "Data/Blending/PCGExPropertiesBlender.h"




#include "PCGExNeighborSampleProperties.generated.h"

///


USTRUCT(BlueprintType)
struct FPCGExPropertiesSamplerConfigBase
{
	GENERATED_BODY()

	FPCGExPropertiesSamplerConfigBase()
	{
	}

	/** Properties blending */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExPropertiesBlendingDetails Blending = FPCGExPropertiesBlendingDetails(EPCGExDataBlendingType::None);
};


/**
 * 
 */
class FPCGExNeighborSampleProperties : public FPCGExNeighborSampleOperation
{
public:
	FPCGExPropertiesBlendingDetails BlendingDetails;

	virtual void PrepareForCluster(FPCGExContext* InContext, TSharedRef<PCGExCluster::FCluster> InCluster, TSharedRef<PCGExData::FFacade> InVtxDataFacade, TSharedRef<PCGExData::FFacade> InEdgeDataFacade) override;

	virtual void PrepareNode(const PCGExCluster::FNode& TargetNode) const override;

	virtual void SampleNeighborNode(const PCGExCluster::FNode& TargetNode, const PCGExGraph::FLink Lk, const double Weight) override;
	virtual void SampleNeighborEdge(const PCGExCluster::FNode& TargetNode, const PCGExGraph::FLink Lk, const double Weight) override;

	virtual void FinalizeNode(const PCGExCluster::FNode& TargetNode, const int32 Count, const double TotalWeight) override;

protected:
	TUniquePtr<PCGExDataBlending::FPropertiesBlender> PropertiesBlender;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExNeighborSamplerFactoryProperties : public UPCGExNeighborSamplerFactoryData
{
	GENERATED_BODY()

public:
	FPCGExPropertiesSamplerConfigBase Config;
	virtual TSharedPtr<FPCGExNeighborSampleOperation> CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|NeighborSample")
class UPCGExNeighborSamplePropertiesSettings : public UPCGExNeighborSampleProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NeighborSamplerProperties, "Sampler : Vtx Properties", "Create a single neighbor attribute sampler, to be used by a Sample Neighbors node.",
		PCGEX_FACTORY_NAME_PRIORITY)

#endif
	//~End UPCGSettings

	//~Begin UPCGExNeighborSampleProviderSettings
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
	//~End UPCGExNeighborSampleProviderSettings

	/** Sampler Settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExPropertiesSamplerConfigBase Config;
};
