// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampling.h"
#include "Data/PCGSplineData.h"

#include "PCGExSampleNearestSpline.generated.h"

#define PCGEX_FOREACH_FIELD_NEARESTPOLYLINE(MACRO)\
MACRO(Success, bool)\
MACRO(Transform, FTransform)\
MACRO(LookAtTransform, FTransform)\
MACRO(Distance, double)\
MACRO(SignedDistance, double)\
MACRO(Angle, double)\
MACRO(Time, double)\
MACRO(NumInside, int32)\
MACRO(NumSamples, int32)

class UPCGExFilterFactoryBase;

namespace PCGExPolyLine
{
	struct PCGEXTENDEDTOOLKIT_API FSampleInfos
	{
		FSampleInfos()
		{
		}

		FSampleInfos(const FTransform& InTransform, const double InDistance, const double InTime):
			Transform(InTransform), Distance(InDistance), Time(InTime)
		{
		}

		FTransform Transform;
		double Distance = 0;
		double Time = 0;
	};

	struct PCGEXTENDEDTOOLKIT_API FTargetsCompoundInfos
	{
		FTargetsCompoundInfos()
		{
		}

		int32 NumTargets = 0;
		double TotalWeight = 0;
		double SampledRangeMin = TNumericLimits<double>::Max();
		double SampledRangeMax = 0;
		double SampledRangeWidth = 0;
		int32 UpdateCount = 0;

		FSampleInfos Closest;
		FSampleInfos Farthest;

		FORCEINLINE void UpdateCompound(const FSampleInfos& Infos)
		{
			UpdateCount++;

			if (Infos.Distance < SampledRangeMin)
			{
				Closest = Infos;
				SampledRangeMin = Infos.Distance;
			}

			if (Infos.Distance > SampledRangeMax)
			{
				Farthest = Infos;
				SampledRangeMax = Infos.Distance;
			}

			SampledRangeWidth = SampledRangeMax - SampledRangeMin;
		}

		FORCEINLINE double GetRangeRatio(const double Distance) const
		{
			return FMath::Clamp(Distance - SampledRangeMin, 0, SampledRangeWidth) / SampledRangeWidth;
		}

		bool IsValid() const { return UpdateCount > 0; }
	};
}

/**
 * Use PCGExSampling to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExSampleNearestSplineSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExSampleNearestSplineSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleNearestSpline, "Sample : Nearest Spline", "Find the closest transform on nearest polylines.");
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
	EPCGExSampleMethod SampleMethod = EPCGExSampleMethod::WithinRange;

	/** Minimum target range. Used as fallback if LocalRangeMin is enabled but missing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, ClampMin=0))
	double RangeMin = 0;

	/** Maximum target range. Used as fallback if LocalRangeMax is enabled but missing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, ClampMin=0))
	double RangeMax = 300;

	/** Use a per-point minimum range*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalRangeMin = false;

	/** Attribute or property to read the minimum range from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, EditCondition="bUseLocalRangeMin", EditConditionHides))
	FPCGAttributePropertyInputSelector LocalRangeMin;

	/** Use a per-point maximum range*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalRangeMax = false;

	/** Attribute or property to read the maximum range from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, EditCondition="bUseLocalRangeMax", EditConditionHides))
	FPCGAttributePropertyInputSelector LocalRangeMax;

	/** Distance method to be used for source points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	EPCGExDistance DistanceSettings = EPCGExDistance::Center;

	/** Weight method used for blending */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable))
	EPCGExRangeType WeightMethod = EPCGExRangeType::FullRange;

	/** Curve that balances weight over distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable))
	TSoftObjectPtr<UCurveFloat> WeightOverDistance;

	/** Write whether the sampling was sucessful or not to a boolean attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteSuccess = false;

	/** Name of the 'boolean' attribute to write sampling success to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteSuccess"))
	FName SuccessAttributeName = FName("bSamplingSuccess");

	/** Write the sampled transform. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteTransform = false;

	/** Name of the 'transform' attribute to write sampled Transform to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteTransform"))
	FName TransformAttributeName = FName("WeightedTransform");


	/** Write the sampled transform. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteLookAtTransform = false;

	/** Name of the 'transform' attribute to write sampled Transform to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteLookAtTransform"))
	FName LookAtTransformAttributeName = FName("WeightedLookAt");

	/** The axis to align transform the look at vector to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Align", EditCondition="bWriteLookAtTransform", EditConditionHides, HideEditConditionToggle))
	EPCGExAxisAlign LookAtAxisAlign = EPCGExAxisAlign::Forward;

	/** Up vector source.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Use Up from...", EditCondition="bWriteLookAtTransform", EditConditionHides, HideEditConditionToggle))
	EPCGExSampleSource LookAtUpSelection = EPCGExSampleSource::Constant;

	/** The attribute or property on selected source to use as Up vector for the look at transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Up Vector", EditCondition="bWriteLookAtTransform && LookAtUpSelection==EPCGExSampleSource::Source", EditConditionHides, HideEditConditionToggle))
	FPCGAttributePropertyInputSelector LookAtUpSource;

	/** The axis on the target to use as Up vector for the look at transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Up Vector", EditCondition="bWriteLookAtTransform && LookAtUpSelection==EPCGExSampleSource::Target", EditConditionHides, HideEditConditionToggle))
	EPCGExAxis LookAtUpAxis = EPCGExAxis::Up;

	/** The constant to use as Up vector for the look at transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Up Vector", EditCondition="bWriteLookAtTransform && LookAtUpSelection==EPCGExSampleSource::Constant", EditConditionHides, HideEditConditionToggle))
	FVector LookAtUpConstant = FVector::UpVector;


	/** Write the sampled distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDistance = false;

	/** Name of the 'double' attribute to write sampled distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteDistance"))
	FName DistanceAttributeName = FName("WeightedDistance");

	/** Write the sampled Signed distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteSignedDistance = false;

	/** Name of the 'double' attribute to write sampled Signed distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteSignedDistance"))
	FName SignedDistanceAttributeName = FName("WeightedSignedDistance");

	/** Axis to use to calculate the distance' sign*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Axis", EditCondition="bWriteSignedDistance", EditConditionHides, HideEditConditionToggle))
	EPCGExAxis SignAxis = EPCGExAxis::Forward;

	/** Write the sampled angle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteAngle = false;

	/** Name of the 'double' attribute to write sampled Signed distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteAngle"))
	FName AngleAttributeName = FName("WeightedAngle");

	/** Axis to use to calculate the angle*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Axis", EditCondition="bWriteAngle", EditConditionHides, HideEditConditionToggle))
	EPCGExAxis AngleAxis = EPCGExAxis::Forward;

	/** Unit/range to output the angle to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Range", EditCondition="bWriteAngle", EditConditionHides, HideEditConditionToggle))
	EPCGExAngleRange AngleRange = EPCGExAngleRange::PIRadians;

	/** Write the sampled time (spline space). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteTime = false;

	/** Name of the 'double' attribute to write sampled spline Time to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteTime"))
	FName TimeAttributeName = FName("WeightedTime");

	/** Write the inside/outside status of the point toward any sampled spline. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNumInside = false;

	/** Name of the 'int32' attribute to write the number of spline this point lies inside*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteNumInside"))
	FName NumInsideAttributeName = FName("NumInside");

	/** Write the sampled distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNumSamples = false;

	/** Name of the 'int32' attribute to write the number of sampled neighbors to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteNumSamples"))
	FName NumSamplesAttributeName = FName("NumSamples");
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSampleNearestSplineContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExSampleNearestSplineElement;

	virtual ~FPCGExSampleNearestSplineContext() override;

	TArray<UPCGSplineData*> Targets;

	int64 NumTargets = 0;

	TObjectPtr<UCurveFloat> WeightCurve = nullptr;

	PCGEX_FOREACH_FIELD_NEARESTPOLYLINE(PCGEX_OUTPUT_DECL_TOGGLE)
};

class PCGEXTENDEDTOOLKIT_API FPCGExSampleNearestSplineElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExSampleNearestSpline
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		FPCGExSampleNearestSplineContext* LocalTypedContext = nullptr;
		const UPCGExSampleNearestSplineSettings* LocalSettings = nullptr;

		PCGExData::FCache<double>* RangeMinGetter = nullptr;
		PCGExData::FCache<double>* RangeMaxGetter = nullptr;
		PCGExData::FCache<FVector>* LookAtUpGetter = nullptr;

		FVector SafeUpVector = FVector::UpVector;

		PCGEX_FOREACH_FIELD_NEARESTPOLYLINE(PCGEX_OUTPUT_DECL)

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}
