// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExGraph.h"
#include "PCGExOperation.h"

#include "Graph/Filters/PCGExClusterFilter.h"

#include "PCGExNeighborSampleFactoryProvider.generated.h"

/// 
#define PCGEX_SAMPLER_CREATE\
	NewOperation->SamplingConfig = SamplingConfig; \
	if (SamplingConfig.bUseLocalCurve){	NewOperation->SamplingConfig.LocalWeightCurve.ExternalCurve = SamplingConfig.WeightCurve.Get(); }\
	NewOperation->WeightCurveObj = SamplingConfig.LocalWeightCurve.GetRichCurveConst();\
	NewOperation->PointFilterFactories.Append(PointFilterFactories); \
	NewOperation->ValueFilterFactories.Append(ValueFilterFactories);

namespace PCGExNeighborSample
{
	const FName SourceSamplersLabel = TEXT("Samplers");
	const FName OutputSamplerLabel = TEXT("Sampler");
}

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSamplingConfig
{
	GENERATED_BODY()

	FPCGExSamplingConfig()
	{
		LocalWeightCurve.EditorCurveData.AddKey(0, 0);
		LocalWeightCurve.EditorCurveData.AddKey(1, 1);
	}

	/** Type of range for weight blending computation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExRangeType RangeType = EPCGExRangeType::FullRange;

	/** The maximum sampling traversal depth */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=1))
	int32 MaxDepth = 1;

	/** How to compute the initial blend weight */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExBlendOver BlendOver = EPCGExBlendOver::Index;

	/** Max dist */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="BlendOver==EPCGExBlendOver::Distance", EditConditionHides, ClampMin=0.001))
	double MaxDistance = 300;

	/** The fixed blending value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0.001, EditCondition="BlendOver==EPCGExBlendOver::Fixed", EditConditionHides))
	double FixedBlend = 1;

	/** Whether to use in-editor curve or an external asset. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayPriority=-1))
	bool bUseLocalCurve = false;

	// TODO: DirtyCache for OnDependencyChanged when this float curve is an external asset
	/** Curve over which the blending weight will be remapped  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, DisplayName="Weight Curve", EditCondition = "bUseLocalCurve", EditConditionHides))
	FRuntimeFloatCurve LocalWeightCurve;

	/** Curve over which the blending weight will be remapped  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Weight Curve", EditCondition="!bUseLocalCurve", EditConditionHides))
	TSoftObjectPtr<UCurveFloat> WeightCurve = TSoftObjectPtr<UCurveFloat>(PCGEx::WeightDistributionLinear);

	/** Which type of neighbor to sample */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExClusterComponentSource NeighborSource = EPCGExClusterComponentSource::Vtx;
};


/**
 * 
 */
UCLASS()
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExNeighborSampleOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	TSharedPtr<PCGExClusterFilter::FManager> PointFilters;
	TSharedPtr<PCGExClusterFilter::FManager> ValueFilters;

	TSharedPtr<PCGExData::FFacade> VtxDataFacade;
	TSharedPtr<PCGExData::FFacade> EdgeDataFacade;

	FPCGExSamplingConfig SamplingConfig;

	const FRichCurve* WeightCurveObj = nullptr;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual void PrepareForCluster(FPCGExContext* InContext, TSharedRef<PCGExCluster::FCluster> InCluster, TSharedRef<PCGExData::FFacade> InVtxDataFacade, TSharedRef<PCGExData::FFacade> InEdgeDataFacade);
	virtual bool IsOperationValid();

	TSharedRef<PCGExData::FPointIO> GetSourceIO() const;
	TSharedRef<PCGExData::FFacade> GetSourceDataFacade() const;

	virtual void ProcessNode(const int32 NodeIndex) const;

	FORCEINLINE virtual void PrepareNode(const PCGExCluster::FNode& TargetNode) const
	{
	}

	FORCEINLINE virtual void BlendNodePoint(const PCGExCluster::FNode& TargetNode, const PCGExGraph::FLink Lk, const double Weight) const
	{
	}

	FORCEINLINE virtual void BlendNodeEdge(const PCGExCluster::FNode& TargetNode, const PCGExGraph::FLink Lk, const double Weight) const
	{
	}

	FORCEINLINE virtual void FinalizeNode(const PCGExCluster::FNode& TargetNode, const int32 Count, const double TotalWeight) const
	{
	}

	virtual void CompleteOperation();

	virtual void Cleanup() override;

	TArray<TObjectPtr<const UPCGExFilterFactoryData>> PointFilterFactories;
	TArray<TObjectPtr<const UPCGExFilterFactoryData>> ValueFilterFactories;

protected:
	bool bIsValidOperation = true;
	TSharedPtr<PCGExCluster::FCluster> Cluster;

	FORCEINLINE virtual double SampleCurve(const double InTime) const { return WeightCurveObj->Eval(InTime); }
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExNeighborSamplerFactoryData : public UPCGExFactoryData
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::Sampler; }

	FPCGExSamplingConfig SamplingConfig;

	TArray<TObjectPtr<const UPCGExFilterFactoryData>> PointFilterFactories;
	TArray<TObjectPtr<const UPCGExFilterFactoryData>> ValueFilterFactories;

	virtual UPCGExNeighborSampleOperation* CreateOperation(FPCGExContext* InContext) const;

	virtual void RegisterVtxBuffersDependencies(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, PCGExData::FFacadePreloader& FacadePreloader) const
	{
	}

	virtual void RegisterAssetDependencies(FPCGExContext* InContext) const override;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|NeighborSample")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExNeighborSampleProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NeighborSamplerAttribute, "Sampler : Abstract", "Abstract sampler settings.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorSamplerNeighbor; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

	//~Begin UPCGExFactoryProviderSettings
public:
	virtual FName GetMainOutputPin() const override { return PCGExNeighborSample::OutputSamplerLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
	//~End UPCGExFactoryProviderSettings

	/** Priority for sampling order. Higher values are processed last. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	int32 Priority = 0;

	/** Priority for sampling order. Higher values are processed last. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1, ShowOnlyInnerProperties))
	FPCGExSamplingConfig SamplingConfig;
};
