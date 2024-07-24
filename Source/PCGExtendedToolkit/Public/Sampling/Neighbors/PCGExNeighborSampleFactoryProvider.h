// Copyright Timothé Lapetite 2024
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
	PCGEX_LOAD_SOFTOBJECT(UCurveFloat, NewOperation->SamplingConfig.WeightCurve, NewOperation->WeightCurveObj, PCGEx::WeightDistributionLinear) \
	NewOperation->PointFilterFactories.Append(PointFilterFactories); \
	NewOperation->ValueFilterFactories.Append(ValueFilterFactories);

namespace PCGExDataBlending
{
	class FMetadataBlender;
}

namespace PCGExNeighborSample
{
	const FName SourceSamplersLabel = TEXT("Samplers");
	const FName OutputSamplerLabel = TEXT("Sampler");
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExDistanceSamplingDetails : public FPCGExDistanceDetails
{
	GENERATED_BODY()

	FPCGExDistanceSamplingDetails()
	{
	}

	explicit FPCGExDistanceSamplingDetails(const double InMaxDistance):
		MaxDistance(InMaxDistance)
	{
	}

	~FPCGExDistanceSamplingDetails()
	{
	}

	/** Max distance at which a node can be selected. Use <= 0 to ignore distance check. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double MaxDistance = -1;

	FORCEINLINE bool WithinDistance(const FVector& NodePosition, const FVector& TargetPosition) const
	{
		if (MaxDistance <= 0) { return true; }
		return FVector::Distance(NodePosition, TargetPosition) < MaxDistance;
	}

	FORCEINLINE bool WithinDistanceOut(const FVector& NodePosition, const FVector& TargetPosition, double& OutDistance) const
	{
		OutDistance = FVector::Distance(NodePosition, TargetPosition);
		if (MaxDistance <= 0) { return true; }
		return OutDistance < MaxDistance;
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSamplingConfig
{
	GENERATED_BODY()

	FPCGExSamplingConfig():
		WeightCurve(PCGEx::WeightDistributionLinear)
	{
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

	/** Curve over which the blending weight will be remapped  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UCurveFloat> WeightCurve = TSoftObjectPtr<UCurveFloat>(PCGEx::WeightDistributionLinear);

	/** Which type of neighbor to sample */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExGraphValueSource NeighborSource = EPCGExGraphValueSource::Vtx;
};


/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExNeighborSampleOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	PCGExClusterFilter::TManager* PointFilters = nullptr;
	PCGExClusterFilter::TManager* ValueFilters = nullptr;

	PCGExData::FFacade* VtxDataFacade = nullptr;
	PCGExData::FFacade* EdgeDataFacade = nullptr;

	FPCGExSamplingConfig SamplingConfig;
	TObjectPtr<UCurveFloat> WeightCurveObj = nullptr;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual void PrepareForCluster(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster, PCGExData::FFacade* InVtxDataFacade, PCGExData::FFacade* InEdgeDataFacade);
	virtual bool IsOperationValid();

	PCGExData::FPointIO* GetSourceIO() const;
	PCGExData::FFacade* GetSourceDataFacade() const;

	virtual void ProcessNode(const int32 NodeIndex) const;

	FORCEINLINE virtual void PrepareNode(const PCGExCluster::FNode& TargetNode) const
	{
	}

	FORCEINLINE virtual void BlendNodePoint(const PCGExCluster::FNode& TargetNode, const PCGExCluster::FExpandedNeighbor& Neighbor, const double Weight) const
	{
	}

	FORCEINLINE virtual void BlendNodeEdge(const PCGExCluster::FNode& TargetNode, const PCGExCluster::FExpandedNeighbor& Neighbor, const double Weight) const
	{
	}

	FORCEINLINE virtual void FinalizeNode(const PCGExCluster::FNode& TargetNode, const int32 Count, const double TotalWeight) const
	{
	}

	virtual void FinalizeOperation();

	virtual void Cleanup() override;

	TArray<UPCGExFilterFactoryBase*> PointFilterFactories;
	TArray<UPCGExFilterFactoryBase*> ValueFilterFactories;

protected:
	bool bIsValidOperation = true;
	PCGExCluster::FCluster* Cluster = nullptr;

	FORCEINLINE virtual double SampleCurve(const double InTime) const { return WeightCurveObj->GetFloatValue(InTime); }
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExNeighborSamplerFactoryBase : public UPCGExParamFactoryBase
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::Sampler; }

	FPCGExSamplingConfig SamplingConfig;

	TArray<UPCGExFilterFactoryBase*> PointFilterFactories;
	TArray<UPCGExFilterFactoryBase*> ValueFilterFactories;

	virtual UPCGExNeighborSampleOperation* CreateOperation() const;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|NeighborSample")
class PCGEXTENDEDTOOLKIT_API UPCGExNeighborSampleProviderSettings : public UPCGExFactoryProviderSettings
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
	virtual FName GetMainOutputLabel() const override { return PCGExNeighborSample::OutputSamplerLabel; }
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

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
