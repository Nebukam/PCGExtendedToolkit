// Copyright 2024 Timothé Lapetite and contributors
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
UCLASS(MinimalAPI)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExNeighborSampleAttribute : public UPCGExNeighborSampleOperation
{
	GENERATED_BODY()

public:
	TSharedPtr<PCGExDataBlending::FMetadataBlender> Blender;

	TSet<FName> SourceAttributes;
	EPCGExDataBlendingType Blending = EPCGExDataBlendingType::Average;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual void PrepareForCluster(FPCGExContext* InContext, TSharedRef<PCGExCluster::FCluster> InCluster, TSharedRef<PCGExData::FFacade> InVtxDataFacade, TSharedRef<PCGExData::FFacade> InEdgeDataFacade) override;

	FORCEINLINE virtual void PrepareNode(const PCGExCluster::FNode& TargetNode) const override
	{
		Blender->PrepareForBlending(TargetNode.PointIndex);
	}

	FORCEINLINE virtual void BlendNodePoint(const PCGExCluster::FNode& TargetNode, const PCGExGraph::FLink Lk, const double Weight) const override
	{
		const int32 PrimaryIndex = TargetNode.PointIndex;
		Blender->Blend(PrimaryIndex, Cluster->GetNode(Lk)->PointIndex, PrimaryIndex, Weight);
	}

	FORCEINLINE virtual void BlendNodeEdge(const PCGExCluster::FNode& TargetNode, const PCGExGraph::FLink Lk, const double Weight) const override
	{
		const int32 PrimaryIndex = TargetNode.PointIndex;
		Blender->Blend(PrimaryIndex, Cluster->GetEdge(Lk)->PointIndex, PrimaryIndex, Weight);
	}

	FORCEINLINE virtual void FinalizeNode(const PCGExCluster::FNode& TargetNode, const int32 Count, const double TotalWeight) const override
	{
		const int32 PrimaryIndex = TargetNode.PointIndex;
		Blender->CompleteBlending(PrimaryIndex, Count, TotalWeight);
	}

	virtual void CompleteOperation() override;

	virtual void Cleanup() override;

protected:
	FPCGExBlendingDetails MetadataBlendingDetails;
};


USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAttributeSamplerConfigBase
{
	GENERATED_BODY()

	FPCGExAttributeSamplerConfigBase()
	{
	}

	/** Attribute to sample */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSet<FName> SourceAttributes;

	/** How to blend neighbors */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDataBlendingType Blending = EPCGExDataBlendingType::Average;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExNeighborSamplerFactoryAttribute : public UPCGExNeighborSamplerFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExAttributeSamplerConfigBase Config;
	virtual UPCGExNeighborSampleOperation* CreateOperation(FPCGExContext* InContext) const override;

	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade, PCGExData::FFacadePreloader& FacadePreloader) const override
	{
		if (SamplingConfig.NeighborSource == EPCGExClusterComponentSource::Vtx)
		{
			TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(InDataFacade->GetIn()->Metadata);
			for (FName AttrName : Config.SourceAttributes)
			{
				const PCGEx::FAttributeIdentity* Identity = Infos->Find(AttrName);
				if (!Identity)
				{
					PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Missing attribute: \"{0}\"."), FText::FromName(AttrName)));
					return;
				}
				FacadePreloader.Register(InContext, *Identity);
			}
		}
	}
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|NeighborSample")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExNeighborSampleAttributeSettings : public UPCGExNeighborSampleProviderSettings
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

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Sampler Settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExAttributeSamplerConfigBase Config;
};
