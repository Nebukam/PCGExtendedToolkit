﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"
#include "Curves/CurveFloat.h"
#include "Curves/RichCurve.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampleNearestPath.h"
#include "PCGExSampling.h"
#include "Data/Matching/PCGExMatching.h"

#include "PCGExSampleInsidePath.generated.h"

#define PCGEX_FOREACH_FIELD_INSIDEPATH(MACRO)\
MACRO(Distance, double, 0)\
MACRO(NumInside, int32, 0)\
MACRO(NumSamples, int32, 0)

UENUM()
enum class EPCGExSampleInsidePathOutput : uint8
{
	All         = 0 UMETA(DisplayName = "All", Tooltip="Output all paths, whether they successfully sampled a target or not"),
	SuccessOnly = 1 UMETA(DisplayName = "Success only", Tooltip="Output only paths that have sampled at least a single target point"),
	Split       = 2 UMETA(DisplayName = "Split", Tooltip="Split between two pins")
};

class UPCGExPointFilterFactoryData;

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Sampling", meta=(PCGExNodeLibraryDoc="sampling/nearest-spline-2"))
class UPCGExSampleInsidePathSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExSampleInsidePathSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleInsidePath, "Sample : Inside Path", "Sample the points inside the paths.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorSampling; }
#endif

protected:
	virtual FName GetMainInputPin() const override;
	virtual FName GetMainOutputPin() const override;

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings
	
	virtual PCGExData::EIOInit GetIOPreInitForMainPoints() const override;

public:
	/** If enabled, allows you to filter out which targets get sampled by which data */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExMatchingDetails DataMatching = FPCGExMatchingDetails(EPCGExMatchingDetailsUsage::Sampling);

	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails;

	//

	/** Process inputs.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	EPCGExPathSamplingIncludeMode ProcessInputs = EPCGExPathSamplingIncludeMode::All;

	/** Sampling method.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	EPCGExSampleMethod SampleMethod = EPCGExSampleMethod::WithinRange;

	/** If enabled, will always sample points if they lie inside, even if further away from the edges than the specified max range. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	bool bAlwaysSampleWhenInside = true;

	/** If enabled, will only sample paths if the point lies inside */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	bool bOnlySampleWhenInside = true;

	/** Sort direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta = (PCG_Overridable, EditCondition="SampleMethod == EPCGExSampleMethod::BestCandidate", EditConditionHides))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;

#pragma region Sampling Range

	/** Type of Range Min */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	EPCGExInputValueType RangeMinInput = EPCGExInputValueType::Constant;

	/** Minimum target range to sample targets. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, DisplayName="Range Min (Attr)", EditCondition="RangeMinInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector RangeMinAttribute;

	/** Minimum target range to sample targets. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, EditCondition="RangeMinInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0))
	double RangeMin = 0;

	PCGEX_SETTING_VALUE_DECL(RangeMin, double)

	/** Type of Range Min */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	EPCGExInputValueType RangeMaxInput = EPCGExInputValueType::Constant;

	/** Maximum target range to sample targets. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, DisplayName="Range Max (Attr)", EditCondition="RangeMaxInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector RangeMaxAttribute;

	/** Maximum target range to sample targets. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, EditCondition="RangeMaxInput == EPCGExInputValueType::Constant", ClampMin=0))
	double RangeMax = 300;

	PCGEX_SETTING_VALUE_DECL(RangeMax, double)

	/** If the value is greater than 0, will do a rough vertical check as part of the projected inclusion. 0 is infinite. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_NotOverridable, ClampMin=0))
	double HeightInclusion = 0;

#pragma endregion

	/** Weight method used for blending */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	EPCGExRangeType WeightMethod = EPCGExRangeType::FullRange;

	/** Whether to use in-editor curve or an external asset. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_NotOverridable))
	bool bUseLocalCurve = false;

	// TODO: DirtyCache for OnDependencyChanged when this float curve is an external asset
	/** Curve that balances weight over distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta = (PCG_NotOverridable, DisplayName="Weight Over Distance", EditCondition = "bUseLocalCurve", EditConditionHides))
	FRuntimeFloatCurve LocalWeightOverDistance;

	/** Curve that balances weight over distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, EditCondition="!bUseLocalCurve", EditConditionHides))
	TSoftObjectPtr<UCurveFloat> WeightOverDistance;

	/** If enabled, will only output paths that have at least sampled one target point */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_NotOverridable))
	EPCGExSampleInsidePathOutput OutputMode = EPCGExSampleInsidePathOutput::All;

	/** Write whether the sampling was sucessful or not to a boolean attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteSuccess = false;

	/** Name of the 'boolean' attribute to write sampling success to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Success", PCG_Overridable, EditCondition="bWriteSuccess"))
	FName SuccessAttributeName = FName("@Data.bSamplingSuccess");


	/** Write the sampled distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDistance = false;

	/** Name of the 'double' attribute to write sampled distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Distance", PCG_Overridable, EditCondition="bWriteDistance"))
	FName DistanceAttributeName = FName("@Data.WeightedDistance");


	/** Write the inside/outside status of the point toward any sampled spline. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNumInside = false;

	/** Name of the 'int32' attribute to write the number of spline this point lies inside*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="NumInside", PCG_Overridable, EditCondition="bWriteNumInside"))
	FName NumInsideAttributeName = FName("@Data.NumInside");

	/** Only increment num inside count when comes from a bClosedLoop spline. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Only if Closed Path", EditCondition="bWriteNumInside && ProcessInputs == EPCGExPathSamplingIncludeMode::All", EditConditionHides, HideEditConditionToggle))
	bool bOnlyIncrementInsideNumIfClosed = false;

	/** Write the sampled distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNumSamples = false;

	/** Name of the 'int32' attribute to write the number of sampled neighbors to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="NumSamples", PCG_Overridable, EditCondition="bWriteNumSamples"))
	FName NumSamplesAttributeName = FName("@Data.NumSamples");


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIfHasSuccesses = false;

	/** If enabled, add the specified tag to the output data if at least a single spline has been sampled.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIfHasSuccesses"))
	FString HasSuccessesTag = TEXT("HasSuccesses");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIfHasNoSuccesses = false;

	/** If enabled, add the specified tag to the output data if no spline was found within range.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIfHasNoSuccesses"))
	FString HasNoSuccessesTag = TEXT("HasNoSuccesses");

	//

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable), AdvancedDisplay)
	bool bIgnoreSelf = true;
};

struct FPCGExSampleInsidePathContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExSampleInsidePathElement;

	TArray<TObjectPtr<const UPCGExBlendOpFactory>> BlendingFactories;

	TSharedPtr<PCGExSampling::FTargetsHandler> TargetsHandler;
	int32 NumMaxTargets = 0;

	TSharedPtr<PCGExSorting::FPointSorter> Sorter;

	FRuntimeFloatCurve RuntimeWeightCurve;
	const FRichCurve* WeightCurve = nullptr;

	PCGEX_FOREACH_FIELD_INSIDEPATH(PCGEX_OUTPUT_DECL_TOGGLE)

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExSampleInsidePathElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(SampleInsidePath)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExSampleInsidePath
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExSampleInsidePathContext, UPCGExSampleInsidePathSettings>
	{
		TSet<const UPCGData*> IgnoreList;
		TSharedPtr<PCGExPaths::FPolyPath> Path;

		FPCGExGeo2DProjectionDetails ProjectionDetails;
		TSharedPtr<PCGExDetails::FDistances> DistanceDetails;

		double RangeMin = 0;
		double RangeMax = 0;

		double NumSampled = 0;
		int8 bAnySuccess = 0;

		TSharedPtr<PCGExDataBlending::FUnionOpsManager> UnionBlendOpsManager;
		TSharedPtr<PCGExDataBlending::IUnionBlender> DataBlender;

		bool bSingleSample = false;
		bool bClosestSample = false;
		bool bOnlyIncrementInsideNumIfClosed = false;

		// Constants
		TSharedPtr<PCGExSampling::FSampingUnionData> Union;
		TArray<PCGExData::FWeightedPoint> OutWeightedPoints;
		int32 NumInside = 0;
		double WeightedDistance = 0;
		double WeightedTime = 0;
		double WeightedSegmentTime = 0;

		PCGEX_FOREACH_FIELD_INSIDEPATH(PCGEX_OUTPUT_DECL)


		FBox SampleBox = FBox(ForceInit);

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		void ProcessPath();

		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;

		virtual void CompleteWork() override;

		virtual void Cleanup() override;
	};
}
