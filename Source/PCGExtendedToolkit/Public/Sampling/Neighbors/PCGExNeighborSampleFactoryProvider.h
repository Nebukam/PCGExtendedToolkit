// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExGraph.h"

#include "PCGExNeighborSampleFactoryProvider.generated.h"

/// 
#define PCGEX_SAMPLER_CREATE\
	NewOperation->BaseSettings = SamplingSettings;\
	PCGEX_LOAD_SOFTOBJECT(UCurveFloat, NewOperation->BaseSettings.WeightCurve, NewOperation->WeightCurveObj, PCGEx::WeightDistributionLinear)\
	if (!FilterFactories.IsEmpty()) { NewOperation->PointState = new PCGExCluster::FNodeStateHandler(this); }\
	if (ValueStateFactory) { NewOperation->ValueState = new PCGExCluster::FNodeStateHandler(ValueStateFactory); }

namespace PCGExNeighborSample
{
	const FName SourceSamplersLabel = TEXT("Samplers");
	const FName OutputSamplerLabel = TEXT("Sampler");
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExDistanceSamplingSettings : public FPCGExDistanceSettings
{
	GENERATED_BODY()

	FPCGExDistanceSamplingSettings()
	{
	}

	explicit FPCGExDistanceSamplingSettings(const double InMaxDistance):
		MaxDistance(InMaxDistance)
	{
	}

	~FPCGExDistanceSamplingSettings()
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
struct PCGEXTENDEDTOOLKIT_API FPCGExSamplingSettings
{
	GENERATED_BODY()

	FPCGExSamplingSettings():
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
	EPCGExGraphValueSource NeighborSource = EPCGExGraphValueSource::Point;
};


/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExNeighborSampleOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	PCGExDataBlending::FMetadataBlender* Blender = nullptr;

	PCGExCluster::FNodeStateHandler* PointState = nullptr;
	PCGExCluster::FNodeStateHandler* ValueState = nullptr;

	FPCGExSamplingSettings BaseSettings;
	TObjectPtr<UCurveFloat> WeightCurveObj = nullptr;

	virtual bool PrepareForCluster(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster);
	virtual bool IsOperationValid();

	PCGExData::FPointIO& GetSourceIO() const;

	FORCEINLINE virtual void ProcessNodeForPoints(const int32 InNodeIndex) const;
	FORCEINLINE virtual void ProcessNodeForEdges(const int32 InNodeIndex) const;

	FORCEINLINE virtual void PrepareNode(PCGExCluster::FNode& TargetNode) const;
	FORCEINLINE virtual void BlendNodePoint(PCGExCluster::FNode& TargetNode, const PCGExCluster::FNode& OtherNode, const double Weight) const;
	FORCEINLINE virtual void BlendNodeEdge(PCGExCluster::FNode& TargetNode, const int32 InEdgeIndex, const double Weight) const;
	FORCEINLINE virtual void FinalizeNode(PCGExCluster::FNode& TargetNode, const int32 Count, const double TotalWeight) const;

	virtual void FinalizeOperation();

	virtual void Cleanup() override;

protected:
	bool bIsValidOperation = true;
	PCGExCluster::FCluster* Cluster = nullptr;

	FORCEINLINE virtual double SampleCurve(const double InTime) const;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGNeighborSamplerFactoryBase : public UPCGExNodeStateFactory
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override;

	FPCGExSamplingSettings SamplingSettings;
	UPCGExNodeStateFactory* ValueStateFactory = nullptr;
	virtual UPCGExNeighborSampleOperation* CreateOperation() const;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|NeighborSample")
class PCGEXTENDEDTOOLKIT_API UPCGExNeighborSampleProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NeighborSamplerAttribute, "Sampler : Abstract", "Abstract sampler settings.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExEditorSettings>()->NodeColorSamplerNeighbor; }

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
#endif
	//~End UPCGSettings

public:
	virtual FName GetMainOutputLabel() const override;
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Priority for sampling order. Higher values are processed last. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	int32 Priority = 0;

	/** Priority for sampling order. Higher values are processed last. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1, ShowOnlyInnerProperties))
	FPCGExSamplingSettings SamplingSettings;
};
