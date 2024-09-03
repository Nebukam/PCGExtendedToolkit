// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampling.h"
#include "PCGExDetails.h"
#include "Data/Blending/PCGExDataBlending.h"

#include "PCGExSampleNearestBounds.generated.h"

#define PCGEX_FOREACH_FIELD_NEARESTBOUNDS(MACRO)\
MACRO(Success, bool)\
MACRO(Transform, FTransform)\
MACRO(LookAtTransform, FTransform)\
MACRO(Distance, double)\
MACRO(SignedDistance, double)\
MACRO(Angle, double)\
MACRO(NumSamples, int32)

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Sample Method"))
enum class EPCGExBoundsSampleMethod : uint8
{
	WithinRange UMETA(DisplayName = "All", ToolTip="Process all overlapping bounds"),
	ClosestBounds UMETA(DisplayName = "Closest Bounds", ToolTip="Picks & process the closest bounds only"),
	FarthestBounds UMETA(DisplayName = "Farthest Bounds", ToolTip="Picks & process the farthest bounds only"),
	LargestBounds UMETA(DisplayName = "Largest Bounds", ToolTip="Picks & process the largest bounds only (extents length)"),
	SmallestBounds UMETA(DisplayName = "Smallest Bounds", ToolTip="Picks & process the smallest bounds only (extents length)"),
};

namespace PCGExDataBlending
{
	class FMetadataBlender;
}

namespace PCGExDataBlending
{
	struct FPropertiesBlender;
}

class UPCGExFilterFactoryBase;

class UPCGExNodeStateFactory;

namespace PCGExNearestBounds
{
	struct /*PCGEXTENDEDTOOLKIT_API*/ FTargetInfos
	{
		FTargetInfos()
		{
		}

		FTargetInfos(const PCGExGeo::FSample& InSample, const double InLength):
			Index(InSample.BoxIndex),
			Length(InLength),
			Distance(InSample.Distances.Length()),
			Weight(InSample.Weight)
		{
		}

		int32 Index = -1;
		double Length = 0;
		double Distance = 0;
		double Weight = 0;
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FTargetsCompoundInfos
	{
		FTargetsCompoundInfos()
		{
		}

		int32 NumTargets = 0;
		double TotalWeight = 0;
		double SampledRangeMin = TNumericLimits<double>::Max();
		double SampledRangeMax = 0;
		double SampledLengthMin = TNumericLimits<double>::Max();
		double SampledLengthMax = 0;
		int32 UpdateCount = 0;

		FTargetInfos Closest;
		FTargetInfos Farthest;
		FTargetInfos Largest;
		FTargetInfos Smallest;

		FORCEINLINE void UpdateCompound(const FTargetInfos& Infos)
		{
			UpdateCount++;

			if (Infos.Distance < SampledRangeMin)
			{
				Closest = Infos;
				SampledRangeMin = Infos.Distance;
			}
			else if (Infos.Distance > SampledRangeMax)
			{
				Farthest = Infos;
				SampledRangeMax = Infos.Distance;
			}

			if (Infos.Length > SampledLengthMax)
			{
				Largest = Infos;
				SampledLengthMax = Infos.Length;
			}
			else if (Infos.Length < SampledLengthMin)
			{
				Smallest = Infos;
				SampledLengthMin = Infos.Length;
			}
		}

		bool IsValid() const { return UpdateCount > 0; }
	};
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSampleNearestBoundsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExSampleNearestBoundsSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleNearestBounds, "Sample : Nearest Bounds", "Sample nearest target bounds.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorSampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;

	virtual FName GetPointFilterLabel() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** Sampling method.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	EPCGExBoundsSampleMethod SampleMethod = EPCGExBoundsSampleMethod::WithinRange;

	/** Sampling method.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_NotOverridable))
	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledBounds;

	/** Curve that balances weight over distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	TSoftObjectPtr<UCurveFloat> WeightRemap;

	/** Attributes to sample from the targets */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Blending", meta=(PCG_Overridable))
	TMap<FName, EPCGExDataBlendingType> TargetAttributes;

	/** Write the sampled distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Blending", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bBlendPointProperties = false;

	/** The constant to use as Up vector for the look at transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Blending", meta=(PCG_Overridable, EditCondition="bBlendPointProperties"))
	FPCGExPropertiesBlendingDetails PointPropertiesBlendingSettings = FPCGExPropertiesBlendingDetails(EPCGExDataBlendingType::None);

	/** Write whether the sampling was sucessful or not to a boolean attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteSuccess = false;

	/** Name of the 'boolean' attribute to write sampling success to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(DisplayName="Success", PCG_Overridable, EditCondition="bWriteSuccess"))
	FName SuccessAttributeName = FName("bSamplingSuccess");

	/** Write the sampled transform. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteTransform = false;

	/** Name of the 'transform' attribute to write sampled Transform to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(DisplayName="Transform", PCG_Overridable, EditCondition="bWriteTransform"))
	FName TransformAttributeName = FName("WeightedTransform");


	/** Write the sampled transform. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteLookAtTransform = false;

	/** Name of the 'transform' attribute to write sampled Transform to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(DisplayName="LookAt", PCG_Overridable, EditCondition="bWriteLookAtTransform"))
	FName LookAtTransformAttributeName = FName("WeightedLookAt");

	/** The axis to align transform the look at vector to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Align", EditCondition="bWriteLookAtTransform", EditConditionHides, HideEditConditionToggle))
	EPCGExAxisAlign LookAtAxisAlign = EPCGExAxisAlign::Forward;

	/** Up vector source.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Use Up from...", EditCondition="bWriteLookAtTransform", EditConditionHides, HideEditConditionToggle))
	EPCGExSampleSource LookAtUpSelection = EPCGExSampleSource::Constant;

	/** The attribute or property on selected source to use as Up vector for the look at transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Up Vector", EditCondition="bWriteLookAtTransform && LookAtUpSelection!=EPCGExSampleSource::Constant", EditConditionHides, HideEditConditionToggle))
	FPCGAttributePropertyInputSelector LookAtUpSource;

	/** The constant to use as Up vector for the look at transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Up Vector", EditCondition="bWriteLookAtTransform && LookAtUpSelection==EPCGExSampleSource::Constant", EditConditionHides, HideEditConditionToggle))
	FVector LookAtUpConstant = FVector::UpVector;

	/** Write the sampled distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDistance = false;

	/** Name of the 'double' attribute to write sampled distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(DisplayName="Distance", PCG_Overridable, EditCondition="bWriteDistance"))
	FName DistanceAttributeName = FName("WeightedDistance");

	/** Write the sampled Signed distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteSignedDistance = false;

	/** Name of the 'double' attribute to write sampled Signed distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(DisplayName="SignedDistance", PCG_Overridable, EditCondition="bWriteSignedDistance"))
	FName SignedDistanceAttributeName = FName("WeightedSignedDistance");

	/** Axis to use to calculate the distance' sign*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Axis", EditCondition="bWriteSignedDistance", EditConditionHides, HideEditConditionToggle))
	EPCGExAxis SignAxis = EPCGExAxis::Forward;

	/** Write the sampled angle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteAngle = false;

	/** Name of the 'double' attribute to write sampled Signed distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(DisplayName="Angle", PCG_Overridable, EditCondition="bWriteAngle"))
	FName AngleAttributeName = FName("WeightedAngle");

	/** Axis to use to calculate the angle*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Axis", EditCondition="bWriteAngle", EditConditionHides, HideEditConditionToggle))
	EPCGExAxis AngleAxis = EPCGExAxis::Forward;

	/** Unit/range to output the angle to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Range", EditCondition="bWriteAngle", EditConditionHides, HideEditConditionToggle))
	EPCGExAngleRange AngleRange = EPCGExAngleRange::PIRadians;

	/** Write the sampled distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNumSamples = false;

	/** Name of the 'int32' attribute to write the number of sampled neighbors to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(DisplayName="NumSamples", PCG_Overridable, EditCondition="bWriteNumSamples"))
	FName NumSamplesAttributeName = FName("NumSamples");
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSampleNearestBoundsContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExSampleNearestBoundsElement;

	virtual ~FPCGExSampleNearestBoundsContext() override;

	PCGExData::FFacade* BoundsFacade = nullptr;

	FPCGExBlendingDetails BlendingDetails;
	const TArray<FPCGPoint>* BoundsPoints = nullptr;

	TObjectPtr<UCurveFloat> WeightCurve = nullptr;

	PCGEX_FOREACH_FIELD_NEARESTBOUNDS(PCGEX_OUTPUT_DECL_TOGGLE)
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSampleNearestBoundsElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExSampleNearestBounds
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		PCGExGeo::FPointBoxCloud* Cloud = nullptr;
		EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::Bounds;

		bool bSingleSample = false;

		FPCGExSampleNearestBoundsContext* LocalTypedContext = nullptr;
		const UPCGExSampleNearestBoundsSettings* LocalSettings = nullptr;

		PCGExData::FCache<FVector>* LookAtUpGetter = nullptr;

		FVector SafeUpVector = FVector::UpVector;

		PCGExDataBlending::FMetadataBlender* Blender = nullptr;

		PCGEX_FOREACH_FIELD_NEARESTBOUNDS(PCGEX_OUTPUT_DECL)

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints)
			: FPointsProcessor(InPoints)
		{
			DefaultPointFilterValue = true;
		}

		virtual ~FProcessor() override;

		void SamplingFailed(const int32 Index, const FPCGPoint& Point) const;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}
