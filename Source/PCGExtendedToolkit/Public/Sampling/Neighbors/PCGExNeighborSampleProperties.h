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
#include "Data/Blending/PCGExPropertiesBlender.h"


#include "PCGExNeighborSampleProperties.generated.h"

///

/**
 * 
 */
UCLASS(MinimalAPI)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExNeighborSampleProperties : public UPCGExNeighborSampleOperation
{
	GENERATED_BODY()

public:
	FPCGExPropertiesBlendingDetails BlendingDetails;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual void PrepareForCluster(FPCGExContext* InContext, TSharedRef<PCGExCluster::FCluster> InCluster, TSharedRef<PCGExData::FFacade> InVtxDataFacade, TSharedRef<PCGExData::FFacade> InEdgeDataFacade) override;

	FORCEINLINE virtual void PrepareNode(const PCGExCluster::FNode& TargetNode) const override
	{
		FPCGPoint& A = VtxDataFacade->Source->GetMutablePoint(TargetNode.PointIndex);
		PropertiesBlender->PrepareBlending(A, VtxDataFacade->Source->GetInPoint(TargetNode.PointIndex));
	}

	FORCEINLINE virtual void BlendNodePoint(const PCGExCluster::FNode& TargetNode, const PCGExGraph::FLink Lk, const double Weight) const override
	{
		FPCGPoint& A = VtxDataFacade->Source->GetMutablePoint(TargetNode.PointIndex);
		const FPCGPoint& B = VtxDataFacade->Source->GetInPoint(Cluster->GetNode(Lk)->PointIndex);
		PropertiesBlender->Blend(A, B, A, Weight);
	}

	FORCEINLINE virtual void BlendNodeEdge(const PCGExCluster::FNode& TargetNode, const PCGExGraph::FLink Lk, const double Weight) const override
	{
		FPCGPoint& A = VtxDataFacade->Source->GetMutablePoint(TargetNode.PointIndex);
		const FPCGPoint& B = VtxDataFacade->Source->GetInPoint(Cluster->GetEdge(Lk)->PointIndex);
		PropertiesBlender->Blend(A, B, A, Weight);
	}

	FORCEINLINE virtual void FinalizeNode(const PCGExCluster::FNode& TargetNode, const int32 Count, const double TotalWeight) const override
	{
		FPCGPoint& A = VtxDataFacade->Source->GetMutablePoint(TargetNode.PointIndex);
		PropertiesBlender->CompleteBlending(A, Count, TotalWeight);
	}

	virtual void Cleanup() override;

protected:
	TUniquePtr<PCGExDataBlending::FPropertiesBlender> PropertiesBlender;
};


USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPropertiesSamplerConfigBase
{
	GENERATED_BODY()

	FPCGExPropertiesSamplerConfigBase()
	{
	}

	/** Properties blending */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExPropertiesBlendingDetails Blending = FPCGExPropertiesBlendingDetails(EPCGExDataBlendingType::None);
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExNeighborSamplerFactoryProperties : public UPCGExNeighborSamplerFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExPropertiesSamplerConfigBase Config;
	virtual UPCGExNeighborSampleOperation* CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|NeighborSample")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExNeighborSamplePropertiesSettings : public UPCGExNeighborSampleProviderSettings
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
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
	//~End UPCGExNeighborSampleProviderSettings

	/** Sampler Settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExPropertiesSamplerConfigBase Config;
};
