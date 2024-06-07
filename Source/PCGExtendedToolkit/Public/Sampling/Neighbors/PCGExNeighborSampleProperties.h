// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExGraph.h"

#include "PCGExNeighborSampleFactoryProvider.h"

#include "PCGExNeighborSampleProperties.generated.h"

///

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExNeighborSampleProperties : public UPCGExNeighborSampleOperation
{
	GENERATED_BODY()

public:
	FPCGExPropertiesBlendingSettings BlendingSettings;

	virtual bool PrepareForCluster(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster) override;

	virtual void PrepareNode(PCGExCluster::FNode& TargetNode) const override;
	virtual void BlendNodePoint(PCGExCluster::FNode& TargetNode, const PCGExCluster::FNode& OtherNode, const double Weight) const override;
	virtual void BlendNodeEdge(PCGExCluster::FNode& TargetNode, const int32 InEdgeIndex, const double Weight) const override;
	virtual void FinalizeNode(PCGExCluster::FNode& TargetNode, const int32 Count, const double TotalWeight) const override;

	virtual void Cleanup() override;

protected:
	PCGExDataBlending::FPropertiesBlender* Blender = nullptr;
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPropertiesSamplerDescriptorBase
{
	GENERATED_BODY()

	FPCGExPropertiesSamplerDescriptorBase()
	{
	}

	/** Properties blending */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExPropertiesBlendingSettings Blending = FPCGExPropertiesBlendingSettings(EPCGExDataBlendingType::None);
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGNeighborSamplerFactoryProperties : public UPCGNeighborSamplerFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExPropertiesSamplerDescriptorBase Descriptor;
	virtual UPCGExNeighborSampleOperation* CreateOperation() const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|NeighborSample")
class PCGEXTENDEDTOOLKIT_API UPCGExNeighborSamplePropertiesSettings : public UPCGExNeighborSampleProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NeighborSamplerProperties, "Sampler : Properties", "Create a single neighbor attribute sampler, to be used by a Sample Neighbors node.",
		PCGEX_FACTORY_NAME_PRIORITY)

#endif
	//~End UPCGSettings

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Sampler Settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExPropertiesSamplerDescriptorBase Descriptor;
};
