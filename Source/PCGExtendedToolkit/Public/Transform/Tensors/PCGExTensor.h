// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "PCGExDetails.h"
#include "Curves/CurveVector.h"
#include "Data/PCGExData.h"
#include "PCGExTensor.generated.h"

namespace PCGExTensor
{
	struct FTensorSample;
}

UENUM()
enum class EPCGExTensorSamplingMode : uint8
{
	Weighted       = 0 UMETA(DisplayName = "Weighted", ToolTip="Compute a weighted average of the sampled tensors"),
	OrderedInPlace = 1 UMETA(DisplayName = "Ordered (in place)", ToolTip="Applies tensor one after another in order, using the same original position"),
	OrderedMutated = 2 UMETA(DisplayName = "Ordered (mutated)", ToolTip="Applies tensor & update sampling position one after another in order"),
};

UENUM()
enum class EPCGExEffectorFlattenMode : uint8
{
	Weighted         = 0 UMETA(DisplayName = "Weighted", ToolTip="Compute a weighted average of the sampled effectors"),
	Closest          = 1 UMETA(DisplayName = "Closest", ToolTip="Uses the closest effector only"),
	StrongestWeight  = 2 UMETA(DisplayName = "Strongest (Weight)", ToolTip="Uses the effector with the highest weight only"),
	StrongestPotency = 3 UMETA(DisplayName = "Strongest (Potency)", ToolTip="Uses the effector with the highest potency only"),
};

UENUM()
enum class EPCGExEffectorInfluenceShape : uint8
{
	Box    = 0 UMETA(DisplayName = "Box", Tooltip="Point' bounds"),
	Sphere = 1 UMETA(DisplayName = "Sphere", Tooltip="Sphere which radius is defined by the bounds' extents size"),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTensorSamplingMutationsDetails
{
	GENERATED_BODY()

	FPCGExTensorSamplingMutationsDetails()
	{
	}

	virtual ~FPCGExTensorSamplingMutationsDetails()
	{
	}

	/** If enabled, sample will be mirrored. Computed before bidirectional. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	bool bInvert = false;

	/** If enabled, perform a dot product with the direction of the input transform and the resuting sample. If that dot product is < 0, the sampled direction and size is reversed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	bool bBidirectional = false;

	/** Local axis from input transform used to test if the sampled direction should be inverted */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, DisplayName = " └─ Reference Axis", EditCondition="bBidirectional", EditConditionHides))
	EPCGExAxis BidirectionalAxisReference = EPCGExAxis::Forward;

	PCGExTensor::FTensorSample Mutate(const FTransform& InProbe, PCGExTensor::FTensorSample InSample) const;
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTensorConfigBase
{
	GENERATED_BODY()

	explicit FPCGExTensorConfigBase(const bool SupportAttributes = true, const bool SupportMutations = true)
		: bSupportAttributes(SupportAttributes), bSupportMutations(SupportMutations)
	{
		if (!bSupportAttributes)
		{
			PotencyInput = EPCGExInputValueType::Constant;
			WeightInput = EPCGExInputValueType::Constant;
		}

		LocalGuideCurve.VectorCurves[0].AddKey(0, 1);
		LocalGuideCurve.VectorCurves[1].AddKey(0, 0);
		LocalGuideCurve.VectorCurves[2].AddKey(0, 0);

		LocalGuideCurve.VectorCurves[0].AddKey(1, 1);
		LocalGuideCurve.VectorCurves[1].AddKey(1, 0);
		LocalGuideCurve.VectorCurves[2].AddKey(1, 0);

		LocalPotencyFalloffCurve.EditorCurveData.AddKey(1, 0);
		LocalPotencyFalloffCurve.EditorCurveData.AddKey(0, 1);

		LocalWeightFalloffCurve.EditorCurveData.AddKey(1, 0);
		LocalWeightFalloffCurve.EditorCurveData.AddKey(0, 1);

		PotencyAttribute.Update(TEXT("$Density"));
		WeightAttribute.Update(TEXT("Steepness"));
	}

	virtual ~FPCGExTensorConfigBase()
	{
	}

	UPROPERTY(VisibleAnywhere, Category=Settings, meta=(PCG_NotOverridable, HideInDetailPanel, EditCondition="false", EditConditionHides))
	bool bSupportAttributes = true;

	UPROPERTY(VisibleAnywhere, Category=Settings, meta=(PCG_NotOverridable, HideInDetailPanel, EditCondition="false", EditConditionHides))
	bool bSupportMutations = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayPriority=-1))
	double TensorWeight = 1;

	/** How individual effectors on that tensor are composited */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, DisplayPriority=-1))
	EPCGExEffectorFlattenMode Compositing = EPCGExEffectorFlattenMode::Weighted;

	// Guide falloff

	/** Whether to use in-editor curve or an external asset. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Guide", meta=(PCG_NotOverridable))
	bool bUseLocalGuideCurve = true;

	// TODO: DirtyCache for OnDependencyChanged when this float curve is an external asset
	/** Per-point Guide curve sampled using distance to effector origin. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Guide", meta = (PCG_NotOverridable, DisplayName="Guide Curve", EditCondition = "bUseLocalGuideCurve", EditConditionHides))
	FRuntimeVectorCurve LocalGuideCurve;

	/** Per-point Weight falloff curve sampled using distance to effector origin. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Guide", meta=(PCG_Overridable, DisplayName="Guide Curve", EditCondition="!bUseLocalGuideCurve", EditConditionHides))
	TSoftObjectPtr<UCurveVector> GuideCurve;

	// Potency Falloff

	/** Per-point internal Weight input type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Potency", meta = (PCG_NotOverridable, EditCondition="bSupportAttributes", EditConditionHides, DisplayPriority=-1, HideEditConditionToggle))
	EPCGExInputValueType PotencyInput = EPCGExInputValueType::Attribute;

	/** Constant Potency. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Potency", meta=(PCG_Overridable, DisplayName="Potency", EditCondition = "PotencyInput == EPCGExInputValueType::Constant", EditConditionHides, DisplayPriority=-1))
	double Potency = 1;

	/** Per-point Potency. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Potency", meta=(PCG_Overridable, DisplayName="Potency", EditCondition = "bSupportAttributes && PotencyInput != EPCGExInputValueType::Constant", EditConditionHides, DisplayPriority=-1, HideEditConditionToggle))
	FPCGAttributePropertyInputSelector PotencyAttribute;

	/** Whether to use in-editor curve or an external asset. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Potency", meta=(PCG_NotOverridable, DisplayPriority=-1))
	bool bUseLocalPotencyFalloffCurve = true;

	// TODO: DirtyCache for OnDependencyChanged when this float curve is an external asset
	/** Per-point Potency falloff curve sampled using distance to effector origin. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Potency", meta = (PCG_NotOverridable, DisplayName="Potency Falloff Curve", EditCondition = "bUseLocalPotencyFalloffCurve", EditConditionHides, DisplayPriority=-1))
	FRuntimeFloatCurve LocalPotencyFalloffCurve;

	/** Per-point Potency falloff curve sampled using distance to effector origin. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Potency", meta=(PCG_Overridable, DisplayName="Potency Falloff Curve", EditCondition="!bUseLocalPotencyFalloffCurve", EditConditionHides, DisplayPriority=-1))
	TSoftObjectPtr<UCurveFloat> PotencyFalloffCurve;

	/** A multiplier applied to Potency after it's computed. Makes it easy to scale entire tensors up or down, or invert their influence altogether. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Potency", meta=(PCG_Overridable, DisplayName="Potency Scale", EditCondition = "PotencyInput != EPCGExInputValueType::Constant", EditConditionHides, DisplayPriority=-1))
	double PotencyScale = 1;

	const FRichCurve* PotencyFalloffCurveObj = nullptr;

	// Weight Falloff

	/** Per-point internal Weight input type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta = (PCG_NotOverridable, EditCondition="bSupportAttributes", EditConditionHides, DisplayPriority=-1, HideEditConditionToggle))
	EPCGExInputValueType WeightInput = EPCGExInputValueType::Constant;

	/** Per-point internal Weight Constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable, DisplayName="Weight", EditCondition="WeightInput == EPCGExInputValueType::Constant", EditConditionHides, DisplayPriority=-1, ClampMin=0))
	double Weight = 1;

	/** Per-point internal Weight Attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable, DisplayName="Weight", EditCondition="bSupportAttributes && WeightInput != EPCGExInputValueType::Constant", EditConditionHides, DisplayPriority=-1, HideEditConditionToggle))
	FPCGAttributePropertyInputSelector WeightAttribute;

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

	/** How should overlapping effector influence be flattened (not implemented yet)*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Potency", meta=(PCG_NotOverridable, DisplayPriority=-1))
	EPCGExEffectorFlattenMode EffectorFlattenMode = EPCGExEffectorFlattenMode::Weighted;

	/** Tensor mutations settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Sampling Mutations", EditCondition="bSupportMutations", EditConditionHides, HideEditConditionToggle))
	FPCGExTensorSamplingMutationsDetails Mutations;

	virtual void Init();
};

namespace PCGExTensor
{
	const FName OutputTensorLabel = TEXT("Tensor");
	const FName SourceTensorsLabel = TEXT("Tensors");
	const FName SourceEffectorsLabel = TEXT("Effectors");
	const FName SourceTensorConfigSourceLabel = TEXT("Parent Tensor");

	struct FTensorSample
	{
		FVector DirectionAndSize = FVector::ZeroVector;
		FQuat Rotation = FQuat::Identity;
		int32 Effectors = 0; // number of things that affected this sample
		double Weight = 0;   // total weights applied to this sample

		FTensorSample() = default;
		~FTensorSample() = default;
	};

	struct FEffectorMetrics
	{
		double Distance = 0;
		double Factor = 0;
		double Potency = 0;
		double Weight = 0;
		FVector Guide = FVector::ForwardVector;

		FEffectorMetrics() = default;
		~FEffectorMetrics() = default;
	};

	struct FEffectorSample
	{
		FVector Direction = FVector::ZeroVector; // effector direction
		double Potency = 0;                      // i.e, length
		double Weight = 0;                       // weight of this sample

		FEffectorSample() = default;

		FEffectorSample(const FVector& InDirection, const double InPotency, const double InWeight)
			: Direction(InDirection), Potency(InPotency), Weight(InWeight)
		{
		}

		~FEffectorSample() = default;
	};

	struct FEffectorSamples
	{
		FTensorSample TensorSample = FTensorSample();
		TArray<FEffectorSample> Samples;

		double TotalPotency = 0;

		FEffectorSamples() = default;
		~FEffectorSamples() = default;

		FEffectorSample& Emplace_GetRef(const FVector& InDirection, const double InPotency, const double InWeight);

		template <EPCGExEffectorFlattenMode Mode = EPCGExEffectorFlattenMode::Weighted>
		FTensorSample Flatten(double InWeight)
		{
			TensorSample.Effectors = Samples.Num();

			if constexpr (Mode == EPCGExEffectorFlattenMode::Closest)
			{
				FVector DirectionAndSize = FVector::ZeroVector;

				for (const FEffectorSample& EffectorSample : Samples)
				{
					const double S = (EffectorSample.Potency * (EffectorSample.Weight / TensorSample.Weight));
					DirectionAndSize += EffectorSample.Direction * S;
				}

				TensorSample.DirectionAndSize = DirectionAndSize;
				TensorSample.Rotation = FRotationMatrix::MakeFromX(DirectionAndSize.GetSafeNormal()).ToQuat();
				TensorSample.Weight = InWeight;
			}
			else
			{
				// defaults to weighted 
				FVector DirectionAndSize = FVector::ZeroVector;

				for (const FEffectorSample& EffectorSample : Samples)
				{
					const double S = (EffectorSample.Potency * (EffectorSample.Weight / TensorSample.Weight));
					DirectionAndSize += EffectorSample.Direction * S;
				}

				TensorSample.DirectionAndSize = DirectionAndSize;
				TensorSample.Rotation = FRotationMatrix::MakeFromX(DirectionAndSize.GetSafeNormal()).ToQuat();
				TensorSample.Weight = InWeight;
			}

			return TensorSample;
		}
	};
}
