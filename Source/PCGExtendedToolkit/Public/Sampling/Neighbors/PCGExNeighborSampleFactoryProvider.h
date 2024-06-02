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

class UPCGExNeighborSampleOperation;

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSamplerDescriptorBase
{
	GENERATED_BODY()

	FPCGExSamplerDescriptorBase():
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

	/** Attribute to sample */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSet<FName> SourceAttributes;

	/** Whether to write the sampled attribute to another one (as opposed to overwriting the source) */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	//bool bOutputToNewAttribute = false;

	/** Target attribute to write the sampled result to */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bOutputToNewAttribute"))
	//FName TargetAttribute;

	/** How to blend neighbors */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDataBlendingType Blending = EPCGExDataBlendingType::Average;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGNeighborSamplerFactoryBase : public UPCGExNodeStateFactory
{
	GENERATED_BODY()

public:
	FPCGExSamplerDescriptorBase Descriptor;
	UPCGExNodeStateFactory* ValueStateFactory = nullptr;
	virtual UPCGExNeighborSampleOperation* CreateOperation() const;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|NeighborSample")
class PCGEXTENDEDTOOLKIT_API UPCGExNeighborSampleProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NeighborSampler, "Attribute Sampler", "Create a single neighbor attribute sampler, to be used by a Sample Neighbors node.",
		FName(GetDisplayName()))
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

	/** Sampler Settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExSamplerDescriptorBase SamplerSettings;

};
