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

#include "PCGExNeighborSampleAttribute.generated.h"

///

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExNeighborSampleAttribute : public UPCGExNeighborSampleOperation
{
	GENERATED_BODY()

public:
	PCGExDataBlending::FMetadataBlender* Blender = nullptr;

	TSet<FName> SourceAttributes;
	EPCGExDataBlendingType Blending = EPCGExDataBlendingType::Average;

	virtual void PrepareForCluster(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster) override;

	FORCEINLINE virtual void PrepareNode(PCGExCluster::FNode& TargetNode) const override;
	FORCEINLINE virtual void BlendNodePoint(PCGExCluster::FNode& TargetNode, const PCGExCluster::FNode& OtherNode, const double Weight) const override;
	FORCEINLINE virtual void BlendNodeEdge(PCGExCluster::FNode& TargetNode, const int32 InEdgeIndex, const double Weight) const override;
	FORCEINLINE virtual void FinalizeNode(PCGExCluster::FNode& TargetNode, const int32 Count, const double TotalWeight) const override;

	virtual void FinalizeOperation() override;

	virtual void Cleanup() override;

protected:
	FPCGExBlendingSettings MetadataBlendingSettings;
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSamplerDescriptorBase
{
	GENERATED_BODY()

	FPCGExSamplerDescriptorBase()
	{
	}

	/** Attribute to sample */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSet<FName> SourceAttributes;

	/** How to blend neighbors */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDataBlendingType Blending = EPCGExDataBlendingType::Average;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGNeighborSamplerFactoryAttribute : public UPCGNeighborSamplerFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExSamplerDescriptorBase Descriptor;
	virtual UPCGExNeighborSampleOperation* CreateOperation() const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|NeighborSample")
class PCGEXTENDEDTOOLKIT_API UPCGExNeighborSampleAttributeSettings : public UPCGExNeighborSampleProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NeighborSamplerAttribute, "Sampler : Attributes", "Create a single neighbor attribute sampler, to be used by a Sample Neighbors node.",
		FName(GetDisplayName()))

#endif
	//~End UPCGSettings

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Sampler Settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExSamplerDescriptorBase Descriptor;
};
