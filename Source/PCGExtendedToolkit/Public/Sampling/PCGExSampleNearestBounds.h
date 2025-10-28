﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Curves/CurveFloat.h"
#include "Curves/RichCurve.h"

#include "PCGExGlobalSettings.h"
#include "PCGExPointsProcessor.h"
#include "PCGExSampling.h"
#include "PCGExSorting.h"
#include "Data/PCGExPointFilter.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Data/Matching/PCGExMatching.h"

#include "PCGExSampleNearestBounds.generated.h"

#define PCGEX_FOREACH_FIELD_NEARESTBOUNDS(MACRO)\
MACRO(Success, bool, false)\
MACRO(Transform, FTransform, FTransform::Identity)\
MACRO(LookAtTransform, FTransform, FTransform::Identity)\
MACRO(Distance, double, 0)\
MACRO(SignedDistance, double, 0)\
MACRO(ComponentWiseDistance, FVector, FVector::ZeroVector)\
MACRO(Angle, double, 0)\
MACRO(NumSamples, int32, 0)\
MACRO(SampledIndex, int32, -1)

class UPCGExBlendOpFactory;

namespace PCGExMT
{
	template <typename T>
	class TScopedNumericValue;
}

namespace PCGExDataBlending
{
	class IUnionBlender;
	class FUnionOpsManager;
	class FUnionBlender;
}

UENUM()
enum class EPCGExBoundsSampleMethod : uint8
{
	WithinRange    = 0 UMETA(DisplayName = "All", ToolTip="Process all overlapping bounds"),
	ClosestBounds  = 1 UMETA(DisplayName = "Closest Bounds", ToolTip="Picks & process the closest bounds only"),
	FarthestBounds = 2 UMETA(DisplayName = "Farthest Bounds", ToolTip="Picks & process the farthest bounds only"),
	LargestBounds  = 3 UMETA(DisplayName = "Largest Bounds", ToolTip="Picks & process the largest bounds only (extents length)"),
	SmallestBounds = 4 UMETA(DisplayName = "Smallest Bounds", ToolTip="Picks & process the smallest bounds only (extents length)"),
	BestCandidate  = 5 UMETA(DisplayName = "Best Candidate", ToolTip="Picks the best candidate based on sorting rules."),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Sampling", meta=(PCGExNodeLibraryDoc="sampling/nearest-bounds"))
class UPCGExSampleNearestBoundsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExSampleNearestBoundsSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleNearestBounds, "Sample : Nearest Bounds", "Sample nearest target bounds.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorSampling; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	virtual PCGExData::EIOInit GetMainDataInitializationPolicy() const override;

	//~Begin UPCGExPointsProcessorSettings
public:
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourcePointFiltersLabel, "Filters", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings

	/** If enabled, allows you to filter out which targets get sampled by which data */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExMatchingDetails DataMatching = FPCGExMatchingDetails(EPCGExMatchingDetailsUsage::Sampling);

	//

	/** Sampling method.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	EPCGExBoundsSampleMethod SampleMethod = EPCGExBoundsSampleMethod::WithinRange;

	/** Sort direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta = (PCG_Overridable, EditCondition="SampleMethod == EPCGExBoundsSampleMethod::BestCandidate", EditConditionHides))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;

	/** Source bounds.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_NotOverridable))
	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledBounds;

	/** Whether to use in-editor curve or an external asset. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_NotOverridable, DisplayPriority=-1))
	bool bUseLocalCurve = false;

	// TODO: DirtyCache for OnDependencyChanged when this float curve is an external asset
	/** Curve that balances weight over distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta = (PCG_NotOverridable, DisplayName="Weight Remap", EditCondition = "bUseLocalCurve", EditConditionHides))
	FRuntimeFloatCurve LocalWeightRemap;

	/** Curve that balances weight over distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, DisplayName="Weight Remap", EditCondition = "!bUseLocalCurve", EditConditionHides))
	TSoftObjectPtr<UCurveFloat> WeightRemap;

	/** Whether and how to apply sampled result directly (not mutually exclusive with output)*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_NotOverridable))
	FPCGExApplySamplingDetails ApplySampling;


	/** How to blend data from sampled points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Blending", meta=(PCG_Overridable))
	EPCGExBlendingInterface BlendingInterface = EPCGExBlendingInterface::Individual;

	/** Attributes to sample from the targets */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Blending", meta=(PCG_Overridable, EditCondition="BlendingInterface == EPCGExBlendingInterface::Monolithic", EditConditionHides))
	TMap<FName, EPCGExDataBlendingType> TargetAttributes;

	/** Write the sampled distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Blending", meta=(PCG_Overridable, EditCondition="BlendingInterface == EPCGExBlendingInterface::Monolithic", EditConditionHides))
	bool bBlendPointProperties = false;

	/** The constant to use as Up vector for the look at transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Blending", meta=(PCG_Overridable, EditCondition="bBlendPointProperties && BlendingInterface == EPCGExBlendingInterface::Monolithic", EditConditionHides))
	FPCGExPropertiesBlendingDetails PointPropertiesBlendingSettings = FPCGExPropertiesBlendingDetails(EPCGExDataBlendingType::None);


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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" ├─ Align"))
	EPCGExAxisAlign LookAtAxisAlign = EPCGExAxisAlign::Forward;

	/** Up vector source.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" ├─ Use Up from..."))
	EPCGExSampleSource LookAtUpSelection = EPCGExSampleSource::Constant;

	/** The attribute or property on selected source to use as Up vector for the look at transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Up Vector (Attr)", EditCondition="LookAtUpSelection != EPCGExSampleSource::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector LookAtUpSource;

	/** The constant to use as Up vector for the look at transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Up Vector", EditCondition="LookAtUpSelection == EPCGExSampleSource::Constant", EditConditionHides))
	FVector LookAtUpConstant = FVector::UpVector;

	PCGEX_SETTING_VALUE_DECL(LookAtUp, FVector)

	/** Write the sampled distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDistance = false;

	/** Name of the 'double' attribute to write sampled distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Distance", PCG_Overridable, EditCondition="bWriteDistance"))
	FName DistanceAttributeName = FName("WeightedDistance");

	/** Whether to output normalized distance or not*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" ├─ Normalized", EditCondition="bWriteDistance", EditConditionHides, HideEditConditionToggle))
	bool bOutputNormalizedDistance = false;

	/** Whether to do a OneMinus on the normalized distance value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" │ └─ OneMinus", EditCondition="bWriteDistance && bOutputNormalizedDistance", EditConditionHides, HideEditConditionToggle))
	bool bOutputOneMinusDistance = false;

	/** Scale factor applied to the distance output; allows to easily invert it using -1 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Scale", EditCondition="bWriteDistance", EditConditionHides, HideEditConditionToggle))
	double DistanceScale = 1;

	/** Write the sampled Signed distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteSignedDistance = false;

	/** Name of the 'double' attribute to write sampled Signed distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="SignedDistance", PCG_Overridable, EditCondition="bWriteSignedDistance"))
	FName SignedDistanceAttributeName = FName("WeightedSignedDistance");

	/** Axis to use to calculate the distance' sign*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" ├─ Axis", EditCondition="bWriteSignedDistance", EditConditionHides, HideEditConditionToggle))
	EPCGExAxis SignAxis = EPCGExAxis::Forward;

	/** Scale factor applied to the signed distance output; allows to easily invert it using -1 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Scale", EditCondition="bWriteSignedDistance", EditConditionHides, HideEditConditionToggle))
	double SignedDistanceScale = 1;

	/** Write the sampled component-wise distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteComponentWiseDistance = false;

	/** Name of the 'FVector' attribute to write component-wise distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Component Wise Distance", PCG_Overridable, EditCondition="bWriteComponentWiseDistance"))
	FName ComponentWiseDistanceAttributeName = FName("CWDistance");

	/** Whether to output absolute or signed component wise distances */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Absolute", EditCondition="bWriteComponentWiseDistance", EditConditionHides, HideEditConditionToggle))
	bool bAbsoluteComponentWiseDistance = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteAngle = false;

	/** Name of the 'double' attribute to write sampled Signed distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Angle", PCG_Overridable, EditCondition="bWriteAngle"))
	FName AngleAttributeName = FName("WeightedAngle");

	/** Axis to use to calculate the angle*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" ├─ Axis", EditCondition="bWriteAngle", EditConditionHides, HideEditConditionToggle))
	EPCGExAxis AngleAxis = EPCGExAxis::Forward;

	/** Unit/range to output the angle to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Range", EditCondition="bWriteAngle", EditConditionHides, HideEditConditionToggle))
	EPCGExAngleRange AngleRange = EPCGExAngleRange::PIRadians;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNumSamples = false;

	/** Name of the 'int32' attribute to write the number of sampled neighbors to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="NumSamples", PCG_Overridable, EditCondition="bWriteNumSamples"))
	FName NumSamplesAttributeName = FName("NumSamples");

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteSampledIndex = false;

	/** Name of the 'int32' attribute to write the sampled index to. Will use the closest index when sampling multiple points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="SampledIndex", PCG_Overridable, EditCondition="bWriteSampledIndex"))
	FName SampledIndexAttributeName = FName("SampledIndex");

	//

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIfHasSuccesses = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIfHasSuccesses"))
	FString HasSuccessesTag = TEXT("HasSuccesses");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIfHasNoSuccesses = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIfHasNoSuccesses"))
	FString HasNoSuccessesTag = TEXT("HasNoSuccesses");

	//

	/** If enabled, mark filtered out points as "failed". Otherwise, just skip the processing altogether. Only uncheck this if you want to ensure existing attribute values are preserved. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable), AdvancedDisplay)
	bool bProcessFilteredOutAsFails = true;

	/** If enabled, points that failed to sample anything will be pruned. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable), AdvancedDisplay)
	bool bPruneFailedSamples = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable), AdvancedDisplay)
	bool bIgnoreSelf = true;
};

struct FPCGExSampleNearestBoundsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExSampleNearestBoundsElement;

	TArray<TObjectPtr<const UPCGExBlendOpFactory>> BlendingFactories;
	TSharedPtr<PCGExDetails::FDistances> DistanceDetails;

	TSharedPtr<PCGExSampling::FTargetsHandler> TargetsHandler;
	int32 NumMaxTargets = 0;

	TArray<TSharedPtr<PCGExGeo::FPointBoxCloud>> Clouds;
	TArray<TSharedPtr<PCGExDetails::TSettingValue<FVector>>> TargetLookAtUpGetters;

	TSharedPtr<PCGExSorting::FPointSorter> Sorter;

	FPCGExApplySamplingDetails ApplySampling;

	FRuntimeFloatCurve RuntimeWeightCurve;
	const FRichCurve* WeightCurve = nullptr;

	PCGEX_FOREACH_FIELD_NEARESTBOUNDS(PCGEX_OUTPUT_DECL_TOGGLE)

	virtual void RegisterAssetDependencies() override;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExSampleNearestBoundsElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(SampleNearestBounds)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual void PostLoadAssetsDependencies(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override;
};

namespace PCGExSampleNearestBounds
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExSampleNearestBoundsContext, UPCGExSampleNearestBoundsSettings>
	{
		TSet<const UPCGData*> IgnoreList;
		EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::Bounds;

		TArray<int8> SamplingMask;

		bool bSingleSample = false;

		FVector SafeUpVector = FVector::UpVector;
		TSharedPtr<PCGExDetails::TSettingValue<FVector>> LookAtUpGetter;

		FPCGExBlendingDetails BlendingDetails;

		TSharedPtr<PCGExDataBlending::FUnionBlender> UnionBlender;
		TSharedPtr<PCGExDataBlending::FUnionOpsManager> UnionBlendOpsManager;
		TSharedPtr<PCGExDataBlending::IUnionBlender> DataBlender;

		TSharedPtr<PCGExMT::TScopedNumericValue<double>> MaxDistanceValue;
		double MaxDistance = 0;

		int8 bAnySuccess = 0;

		PCGEX_FOREACH_FIELD_NEARESTBOUNDS(PCGEX_OUTPUT_DECL)

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
			DefaultPointFilterValue = true;
		}

		virtual ~FProcessor() override;

		void SamplingFailed(const int32 Index);

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;

		virtual void OnPointsProcessingComplete() override;

		virtual void CompleteWork() override;
		virtual void Write() override;

		virtual void Cleanup() override;
	};
}
