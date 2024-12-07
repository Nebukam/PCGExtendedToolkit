// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampling.h"
#include "Data/PCGSplineData.h"
#include "Misc/PCGExSortPoints.h"


#include "PCGExSampleNearestSpline.generated.h"

#define PCGEX_FOREACH_FIELD_NEARESTPOLYLINE(MACRO)\
MACRO(Success, bool, false)\
MACRO(Transform, FTransform, FTransform::Identity)\
MACRO(LookAtTransform, FTransform, FTransform::Identity)\
MACRO(Distance, double, 0)\
MACRO(Depth, double, -1)\
MACRO(SignedDistance, double, 0)\
MACRO(Angle, double, 0)\
MACRO(Time, double, 0)\
MACRO(NumInside, int32, 0)\
MACRO(NumSamples, int32, 0)\
MACRO(ClosedLoop, bool, false)

class UPCGExFilterFactoryBase;

UENUM()
enum class EPCGExSplineSamplingIncludeMode : uint8
{
	All            = 0 UMETA(DisplayName = "All", ToolTip="Sample all input splines"),
	ClosedLoopOnly = 1 UMETA(DisplayName = "Closed loops only", ToolTip="Sample only closed loops"),
	OpenSplineOnly = 2 UMETA(DisplayName = "Open splines only", ToolTip="Sample only open splines"),
};

UENUM()
enum class EPCGExSplineDepthMode : uint8
{
	Min     = 0 UMETA(DisplayName = "Min", ToolTip="..."),
	Max     = 1 UMETA(DisplayName = "Max", ToolTip="..."),
	Average = 2 UMETA(DisplayName = "Average", ToolTip="..."),
};

namespace PCGExPolyLine
{
	struct /*PCGEXTENDEDTOOLKIT_API*/ FSample
	{
		FSample()
		{
		}

		FSample(const FTransform& InTransform, const double InDistance, const double InTime):
			Transform(InTransform), Distance(InDistance), Time(InTime)
		{
		}

		FTransform Transform;
		double Distance = 0;
		double Time = 0;
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FSamplesStats
	{
		FSamplesStats()
		{
		}

		int32 NumTargets = 0;
		double TotalWeight = 0;
		double SampledRangeMin = MAX_dbl;
		double SampledRangeMax = 0;
		double SampledRangeWidth = 0;
		int32 UpdateCount = 0;

		FSample Closest;
		FSample Farthest;

		void Update(const FSample& Infos, bool& IsNewClosest, bool& IsNewFarthest);

		FORCEINLINE double GetRangeRatio(const double Distance) const { return FMath::Clamp(Distance - SampledRangeMin, 0, SampledRangeWidth) / SampledRangeWidth; }
		FORCEINLINE bool IsValid() const { return UpdateCount > 0; }
	};
}

/**
 * Use PCGExSampling to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSampleNearestSplineSettings : public UPCGExPointsProcessorSettings
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
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourcePointFiltersLabel, "Filters", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings

	/** Sample inputs.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	EPCGExSplineSamplingIncludeMode SampleInputs = EPCGExSplineSamplingIncludeMode::All;

	/** Sampling method.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	EPCGExSampleMethod SampleMethod = EPCGExSampleMethod::WithinRange;

	/** Minimum target range. Used as fallback if LocalRangeMin is enabled but missing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, ClampMin=0))
	double RangeMin = 0;

	/** Maximum target range. Used as fallback if LocalRangeMax is enabled but missing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, ClampMin=0))
	double RangeMax = 300;

	/** If enabled, spline scale affect range. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, ClampMin=0))
	bool bSplineScalesRanges = false;

	/** Use a per-point minimum range*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalRangeMin = false;

	/** Attribute or property to read the minimum range from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, EditCondition="bUseLocalRangeMin"))
	FPCGAttributePropertyInputSelector LocalRangeMin;

	/** Use a per-point maximum range*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalRangeMax = false;

	/** Attribute or property to read the maximum range from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, EditCondition="bUseLocalRangeMax"))
	FPCGAttributePropertyInputSelector LocalRangeMax;

	/** Distance method to be used for source points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	EPCGExDistance DistanceSettings = EPCGExDistance::Center;

	/** Weight method used for blending */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable))
	EPCGExRangeType WeightMethod = EPCGExRangeType::FullRange;

	/** Whether to use in-editor curve or an external asset. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditConditionHides))
	bool bUseLocalCurve = false;
	
	// TODO: DirtyCache for OnDependencyChanged when this float curve is an external asset
	/** Curve that balances weight over distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, DisplayName="Weight Over Distance", EditCondition = "bUseLocalCurve", EditConditionHides))
	FRuntimeFloatCurve LocalWeightOverDistance;
	
	/** Curve that balances weight over distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, EditCondition="!bUseLocalCurve", EditConditionHides))
	TSoftObjectPtr<UCurveFloat> WeightOverDistance;

	/** Write whether the sampling was sucessful or not to a boolean attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteSuccess = false;

	/** Name of the 'boolean' attribute to write sampling success to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Success", PCG_Overridable, EditCondition="bWriteSuccess"))
	FName SuccessAttributeName = FName("bSamplingSuccess");

	/** Write the sampled transform. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteTransform = false;

	/** Name of the 'transform' attribute to write sampled Transform to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Transform", PCG_Overridable, EditCondition="bWriteTransform"))
	FName TransformAttributeName = FName("WeightedTransform");


	/** Write the sampled transform. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteLookAtTransform = false;

	/** Name of the 'transform' attribute to write sampled Transform to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="LookAt", PCG_Overridable, EditCondition="bWriteLookAtTransform"))
	FName LookAtTransformAttributeName = FName("WeightedLookAt");

	/** The axis to align transform the look at vector to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Align", EditCondition="bWriteLookAtTransform", EditConditionHides, HideEditConditionToggle))
	EPCGExAxisAlign LookAtAxisAlign = EPCGExAxisAlign::Forward;

	/** Up vector source.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Use Up from...", EditCondition="bWriteLookAtTransform", EditConditionHides, HideEditConditionToggle))
	EPCGExSampleSource LookAtUpSelection = EPCGExSampleSource::Constant;

	/** The attribute or property on selected source to use as Up vector for the look at transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Up Vector", EditCondition="bWriteLookAtTransform && LookAtUpSelection==EPCGExSampleSource::Source", EditConditionHides, HideEditConditionToggle))
	FPCGAttributePropertyInputSelector LookAtUpSource;

	/** The axis on the target to use as Up vector for the look at transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Up Vector", EditCondition="bWriteLookAtTransform && LookAtUpSelection==EPCGExSampleSource::Target", EditConditionHides, HideEditConditionToggle))
	EPCGExAxis LookAtUpAxis = EPCGExAxis::Up;

	/** The constant to use as Up vector for the look at transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Up Vector", EditCondition="bWriteLookAtTransform && LookAtUpSelection==EPCGExSampleSource::Constant", EditConditionHides, HideEditConditionToggle))
	FVector LookAtUpConstant = FVector::UpVector;

	/** Write the sampled distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDistance = false;

	/** Name of the 'double' attribute to write sampled distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Distance", PCG_Overridable, EditCondition="bWriteDistance"))
	FName DistanceAttributeName = FName("WeightedDistance");

	/** Write the sampled Signed distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteSignedDistance = false;

	/** Name of the 'double' attribute to write sampled Signed distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="SignedDistance", PCG_Overridable, EditCondition="bWriteSignedDistance"))
	FName SignedDistanceAttributeName = FName("WeightedSignedDistance");

	/** Axis to use to calculate the distance' sign*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Axis", EditCondition="bWriteSignedDistance", EditConditionHides, HideEditConditionToggle))
	EPCGExAxis SignAxis = EPCGExAxis::Forward;

	/** Only sign the distance if at least one sampled spline is a bClosedLoop spline. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Only if Closed Spline", EditCondition="bWriteSignedDistance && SampleInputs==EPCGExSplineSamplingIncludeMode::All", EditConditionHides, HideEditConditionToggle))
	bool bOnlySignIfClosed = false;

	/** Write the sampled angle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteAngle = false;

	/** Name of the 'double' attribute to write sampled Signed distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Angle", PCG_Overridable, EditCondition="bWriteAngle"))
	FName AngleAttributeName = FName("WeightedAngle");

	/** Axis to use to calculate the angle*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Axis", EditCondition="bWriteAngle", EditConditionHides, HideEditConditionToggle))
	EPCGExAxis AngleAxis = EPCGExAxis::Forward;

	/** Unit/range to output the angle to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Range", EditCondition="bWriteAngle", EditConditionHides, HideEditConditionToggle))
	EPCGExAngleRange AngleRange = EPCGExAngleRange::PIRadians;

	/** Write the sampled time (spline space). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteTime = false;

	/** Name of the 'double' attribute to write sampled spline Time to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Time", PCG_Overridable, EditCondition="bWriteTime"))
	FName TimeAttributeName = FName("WeightedTime");

	/** Write the inside/outside status of the point toward any sampled spline. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNumInside = false;

	/** Name of the 'int32' attribute to write the number of spline this point lies inside*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="NumInside", PCG_Overridable, EditCondition="bWriteNumInside"))
	FName NumInsideAttributeName = FName("NumInside");

	/** Only increment num inside count when comes from a bClosedLoop spline. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Only if Closed Spline", EditCondition="bWriteNumInside && SampleInputs==EPCGExSplineSamplingIncludeMode::All", EditConditionHides, HideEditConditionToggle))
	bool bOnlyIncrementInsideNumIfClosed = false;

	/** Write the sampled distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNumSamples = false;

	/** Name of the 'int32' attribute to write the number of sampled neighbors to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="NumSamples", PCG_Overridable, EditCondition="bWriteNumSamples"))
	FName NumSamplesAttributeName = FName("NumSamples");

	/** Write the whether the sampled spline is closed or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteClosedLoop = false;

	/** Name of the 'bool' attribute to write whether a closed spline was sampled or not.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="ClosedLoop", PCG_Overridable, EditCondition="bWriteClosedLoop"))
	FName ClosedLoopAttributeName = FName("ClosedLoop");

	/** Write the sampled depth. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDepth = false;

	/** Name of the 'double' attribute to write sampled depth to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(DisplayName="Depth", PCG_Overridable, EditCondition="bWriteDepth"))
	FName DepthAttributeName = FName("Depth");

	/** Depth range */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, DisplayName=" └─ Range", EditCondition="bWriteDepth", EditConditionHides, HideEditConditionToggle))
	double DepthRange = 100;

	/** Inverts depth */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, DisplayName=" └─ Invert", EditCondition="bWriteDepth", EditConditionHides, HideEditConditionToggle))
	bool bInvertDepth = false;

	/** Depth mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, DisplayName=" └─ Mode", EditCondition="bWriteDepth", EditConditionHides, HideEditConditionToggle))
	EPCGExSplineDepthMode DepthMode = EPCGExSplineDepthMode::Min;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bTagIfHasSuccesses = false;

	/** If enabled, add the specified tag to the output data if at least a single spline has been sampled.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, EditCondition="bTagIfHasSuccesses"))
	FString HasSuccessesTag = TEXT("HasSuccesses");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bTagIfHasNoSuccesses = false;

	/** If enabled, add the specified tag to the output data if no spline was found within range.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, EditCondition="bTagIfHasNoSuccesses"))
	FString HasNoSuccessesTag = TEXT("HasNoSuccesses");

	//

	/** If enabled, mark filtered out points as "failed". Otherwise, just skip the processing altogether. Only uncheck this if you want to ensure existing attribute values are preserved. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable), AdvancedDisplay)
	bool bProcessFilteredOutAsFails = true;

	/** If enabled, points that failed to sample anything will be pruned. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable), AdvancedDisplay)
	bool bPruneFailedSamples = false;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSampleNearestSplineContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExSampleNearestSplineElement;

	TSharedPtr<PCGExDetails::FDistances> DistanceDetails;

	TArray<const UPCGSplineData*> Targets;
	TArray<FPCGSplineStruct> Splines;
	TArray<double> SegmentCounts;

	int64 NumTargets = 0;

	FRuntimeFloatCurve RuntimeWeightCurve;
	const FRichCurve* WeightCurve = nullptr;

	PCGEX_FOREACH_FIELD_NEARESTPOLYLINE(PCGEX_OUTPUT_DECL_TOGGLE)
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSampleNearestSplineElement final : public FPCGExPointsProcessorElement
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

namespace PCGExSampleNearestSpline
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExSampleNearestSplineContext, UPCGExSampleNearestSplineSettings>
	{
		TSharedPtr<PCGExDetails::FDistances> DistanceDetails;
		
		TArray<int8> SampleState;

		TSharedPtr<PCGExData::TBuffer<double>> RangeMinGetter;
		TSharedPtr<PCGExData::TBuffer<double>> RangeMaxGetter;
		TSharedPtr<PCGExData::TBuffer<FVector>> LookAtUpGetter;

		FVector SafeUpVector = FVector::UpVector;
		int8 bAnySuccess = 0;

		bool bSingleSample = false;
		bool bClosestSample = false;
		bool bOnlySignIfClosed = false;
		bool bOnlyIncrementInsideNumIfClosed = false;

		PCGEX_FOREACH_FIELD_NEARESTPOLYLINE(PCGEX_OUTPUT_DECL)

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope) override;

		void SamplingFailed(const int32 Index, const FPCGPoint& Point, double InDepth = 0);

		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
