// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "PCGExDetails.h"
#include "Data/PCGExData.h"
#include "PCGExTensor.generated.h"

class UPCGExTensorFactoryData;
class UPCGExTensorOperation;

UENUM()
enum class EPCGExTensorSamplingMode : uint8
{
	Weighted       = 0 UMETA(DisplayName = "Weighted", ToolTip="Compute a weighted average of the sampled tensors"),
	OrderedInPlace = 1 UMETA(DisplayName = "Ordered (in place)", ToolTip="Applies tensor one after another in order, using the same original position"),
	OrderedMutated = 2 UMETA(DisplayName = "Ordered (mutated)", ToolTip="Applies tensor & update sampling position one after another in order"),
};

UENUM()
enum class EPCGExEffectorCompositingMode : uint8
{
	Weighted        = 0 UMETA(DisplayName = "Weighted", ToolTip="Compute a weighted average of the sampled effectors"),
	Closest         = 1 UMETA(DisplayName = "Closest", ToolTip="Uses the closest effector only"),
	HighestPriority = 2 UMETA(DisplayName = "Highest Priority", ToolTip="Uses the effector with the highest priority"),
	LowestPriority  = 3 UMETA(DisplayName = "Lowest Priority", ToolTip="Uses the effector with the lowest priority"),
};

UENUM()
enum class EPCGExEffectorInfluenceShape : uint8
{
	Box    = 0 UMETA(DisplayName = "Box", Tooltip="Point' bounds"),
	Sphere = 1 UMETA(DisplayName = "Sphere", Tooltip="Sphere which radius is defined by the bounds' extents size"),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTensorSamplingDetails
{
	GENERATED_BODY()

	FPCGExTensorSamplingDetails()
	{
	}

	virtual ~FPCGExTensorSamplingDetails()
	{
	}

	/** Resolution input type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta = (PCG_NotOverridable))
	EPCGExTensorSamplingMode SamplingMode = EPCGExTensorSamplingMode::Weighted;
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTensorConfigBase
{
	GENERATED_BODY()

	FPCGExTensorConfigBase()
	{
		LocalStrengthFalloffCurve.EditorCurveData.AddKey(1, 0);
		LocalStrengthFalloffCurve.EditorCurveData.AddKey(0, 1);

		LocalWeightFalloffCurve.EditorCurveData.AddKey(1, 0);
		LocalWeightFalloffCurve.EditorCurveData.AddKey(0, 1);

		StrengthAttribute.Update(TEXT("$Density"));
		WeightAttribute.Update(TEXT("Steepness"));
	}

	virtual ~FPCGExTensorConfigBase()
	{
	}

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, DisplayPriority=-1))
	double TensorWeight = 1;

	/** How individual effectors on that tensor are composited */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, DisplayPriority=-1))
	EPCGExEffectorCompositingMode Compositing = EPCGExEffectorCompositingMode::Weighted;

	// Strength Falloff

	/** Per-point strength. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Strength", DisplayPriority=-1))
	FPCGAttributePropertyInputSelector StrengthAttribute;

	/** Whether to use in-editor curve or an external asset. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Strength Falloff", meta=(PCG_NotOverridable, DisplayPriority=-1))
	bool bUseLocalStrengthFalloffCurve = true;

	// TODO: DirtyCache for OnDependencyChanged when this float curve is an external asset
	/** Per-point Strength falloff curve sampled using distance to effector origin. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Strength Falloff", meta = (PCG_NotOverridable, DisplayName="Strength Falloff Curve", EditCondition = "bUseLocalStrengthFalloffCurve", EditConditionHides, DisplayPriority=-1))
	FRuntimeFloatCurve LocalStrengthFalloffCurve;

	/** Per-point Strength falloff curve sampled using distance to effector origin. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Strength Falloff", meta=(PCG_Overridable, DisplayName="Strength Falloff Curve", EditCondition="!bUseLocalStrengthFalloffCurve", EditConditionHides, DisplayPriority=-1))
	TSoftObjectPtr<UCurveFloat> StrengthFalloffCurve;

	const FRichCurve* StrengthFalloffCurveObj = nullptr;

	// Weight Falloff

	/** Per-point internal Weight input type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta = (PCG_NotOverridable, DisplayPriority=-1))
	EPCGExInputValueType WeightInput = EPCGExInputValueType::Constant;

	/** Per-point internal Weight Constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable, DisplayName="Weight", EditCondition="WeightInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0))
	double WeightConstant = 1;

	/** Per-point internal Weight Attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable, DisplayName="Weight", EditCondition="WeightInput != EPCGExInputValueType::Constant", EditConditionHides, DisplayPriority=-1))
	FPCGAttributePropertyInputSelector WeightAttribute;

	/** Uniform weight factor for this tensor. Multiplier applied to individual output values. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable, EditCondition="WeightInput != EPCGExInputValueType::Constant", DisplayPriority=-1))
	double UniformWeightFactor = 1;


	/** Whether to use in-editor curve or an external asset. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_NotOverridable, DisplayPriority=-1))
	bool bUseLocalWeightFalloffCurve = true;

	// TODO: DirtyCache for OnDependencyChanged when this float curve is an external asset
	/** Per-point Weight falloff curve sampled using distance to effector origin. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta = (PCG_NotOverridable, DisplayName="Weight Falloff Curve", EditCondition = "bUseLocalWeightFalloffCurve", EditConditionHides, DisplayPriority=-1))
	FRuntimeFloatCurve LocalWeightFalloffCurve;

	/** Per-point Weight falloff curve sampled using distance to effector origin. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable, DisplayName="Weight Falloff Curve", EditCondition="!bUseLocalWeightFalloffCurve", EditConditionHides, DisplayPriority=-1))
	TSoftObjectPtr<UCurveFloat> WeightFalloffCurve;

	const FRichCurve* WeightFalloffCurveObj = nullptr;

	virtual void Init();
};

namespace PCGExTensor
{
	const FName OutputTensorLabel = TEXT("Tensor");
	const FName SourceTensorsLabel = TEXT("Tensors");

	struct FTensorSample
	{
		FTransform Transform = FTransform::Identity; // sample direction
		int32 Effectors = 0;                         // number of things that affected this sample
		double Weight = 0;                           // total weights applied to this sample

		FTensorSample() = default;
		~FTensorSample() = default;
	};

	struct FEffectorSample
	{
		FVector Direction = FVector::ZeroVector; // effector direction
		double Strength = 0;                     // i.e, length
		double Weight = 0;                       // weight of this sample

		FEffectorSample() = default;

		FEffectorSample(const FVector& InDirection, const double InStrength, const double InWeight)
			: Direction(InDirection), Strength(InStrength), Weight(InWeight)
		{
		}

		~FEffectorSample() = default;
	};

	struct FEffectorSamples
	{
		FTensorSample TensorSample = FTensorSample();
		TArray<FEffectorSample> Samples;

		FEffectorSamples() = default;
		~FEffectorSamples() = default;

		FEffectorSample& Emplace_GetRef(const FVector& InDirection, const double InStrength, const double InWeight);
		FTensorSample Flatten(double InWeight);
	};

	using FTensorSampleCallback = std::function<bool(const FVector&, FTensorSample&)>;

	class FTensorsHandler : public TSharedFromThis<FTensorsHandler>
	{
		TArray<UPCGExTensorOperation*> Operations;

	public:
		FTensorsHandler();
		bool Init(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExTensorFactoryData>>& InFactories);
		bool Init(FPCGExContext* InContext, const FName InPin);

		FTensorSample SampleAtPosition(const FVector& InPosition, bool& OutSuccess) const;
		FTensorSample SampleAtPositionOrderedInPlace(const FVector& InPosition, bool& OutSuccess) const;
		FTensorSample SampleAtPositionOrderedMutated(const FVector& InPosition, bool& OutSuccess) const;
	};
}
