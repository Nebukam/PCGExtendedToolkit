// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampleNearestPath.h"
#include "PCGExSampling.h"
#include "PCGExScopedContainers.h"
#include "Data/PCGSplineData.h"
#include "Data/Blending/PCGExUnionOpsManager.h"

#include "Paths/PCGExPaths.h"


#include "PCGExSampleInsidePath.generated.h"

#define PCGEX_FOREACH_FIELD_InsidePath(MACRO)\
MACRO(Success, bool, false)\
MACRO(Distance, double, 0)\
MACRO(SignedDistance, double, 0)\
MACRO(ComponentWiseDistance, FVector, FVector::ZeroVector)\
MACRO(NumInside, int32, 0)\
MACRO(NumSamples, int32, 0)

class UPCGExFilterFactoryData;

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Sampling", meta=(PCGExNodeLibraryDoc="sampling/nearest-spline-2"))
class UPCGExSampleInsidePathSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExSampleInsidePathSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleInsidePath, "Sample : Inside Path", "Sample the points inside the paths.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorSampler; }
#endif

protected:
	virtual FName GetMainInputPin() const override;
	virtual FName GetMainOutputPin() const override;

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:

	/** Process inputs.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	EPCGExPathSamplingIncludeMode ProcessInputs = EPCGExPathSamplingIncludeMode::All;

	/** Sampling method.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	EPCGExSampleMethod SampleMethod = EPCGExSampleMethod::WithinRange;

	/** If enabled, will always sample points if they lie inside, even if further away from the edges than the specified max range. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_NotOverridable))
	bool bAlwaysSampleWhenInside = true;

	/** If enabled, will only sample paths if the point lies inside */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_NotOverridable))
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

	PCGEX_SETTING_VALUE_GET(RangeMin, double, RangeMinInput, RangeMinAttribute, RangeMin)

	/** Type of Range Min */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	EPCGExInputValueType RangeMaxInput = EPCGExInputValueType::Constant;

	/** Maximum target range to sample targets. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, DisplayName="Range Max (Attr)", EditCondition="RangeMaxInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector RangeMaxAttribute;

	/** Maximum target range to sample targets. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, EditCondition="RangeMaxInput == EPCGExInputValueType::Constant", ClampMin=0))
	double RangeMax = 300;

	PCGEX_SETTING_VALUE_GET(RangeMax, double, RangeMaxInput, RangeMaxAttribute, RangeMax)

#pragma endregion

	/** Distance method to be used for source points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	EPCGExDistance DistanceSettings = EPCGExDistance::Center;

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

	/** Whether and how to apply sampled result directly (not mutually exclusive with output)*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_NotOverridable))
	FPCGExApplySamplingDetails ApplySampling;

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

	/** Write the sampled Signed distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteSignedDistance = false;

	/** Name of the 'double' attribute to write sampled Signed distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="SignedDistance", PCG_Overridable, EditCondition="bWriteSignedDistance"))
	FName SignedDistanceAttributeName = FName("@Data.WeightedSignedDistance");

	/** Axis to use to calculate the distance' sign*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" ├─ Axis", EditCondition="bWriteSignedDistance", EditConditionHides, HideEditConditionToggle))
	EPCGExAxis SignAxis = EPCGExAxis::Forward;

	/** Only sign the distance if at least one sampled spline is a bClosedLoop spline. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" ├─ Only if Closed Spline", EditCondition="bWriteSignedDistance && ProcessInputs == EPCGExPathSamplingIncludeMode::All", EditConditionHides, HideEditConditionToggle))
	bool bOnlySignIfClosed = false;

	/** Scale factor applied to the signed distance output; allows to easily invert it using -1 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Scale", EditCondition="bWriteSignedDistance", EditConditionHides, HideEditConditionToggle))
	double SignedDistanceScale = 1;

	/** Write the sampled component-wise distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteComponentWiseDistance = false;

	/** Name of the 'FVector' attribute to write component-wise distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Component Wise Distance", PCG_Overridable, EditCondition="bWriteComponentWiseDistance"))
	FName ComponentWiseDistanceAttributeName = FName("@Data.CWDistance");

	/** Whether to output absolute or signed component wise distances */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Absolute", EditCondition="bWriteComponentWiseDistance", EditConditionHides, HideEditConditionToggle))
	bool bAbsoluteComponentWiseDistance = true;


	/** Write the inside/outside status of the point toward any sampled spline. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNumInside = false;

	/** Name of the 'int32' attribute to write the number of spline this point lies inside*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(DisplayName="NumInside", PCG_Overridable, EditCondition="bWriteNumInside"))
	FName NumInsideAttributeName = FName("@Data.NumInside");

	/** Only increment num inside count when comes from a bClosedLoop spline. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, DisplayName=" └─ Only if Closed Path", EditCondition="bWriteNumInside && ProcessInputs == EPCGExPathSamplingIncludeMode::All", EditConditionHides, HideEditConditionToggle))
	bool bOnlyIncrementInsideNumIfClosed = false;

	/** Write the sampled distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNumSamples = false;

	/** Name of the 'int32' attribute to write the number of sampled neighbors to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(DisplayName="NumSamples", PCG_Overridable, EditCondition="bWriteNumSamples"))
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

	/** If enabled, mark filtered out points as "failed". Otherwise, just skip the processing altogether. Only uncheck this if you want to ensure existing attribute values are preserved. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable), AdvancedDisplay)
	bool bProcessFilteredOutAsFails = true;

	/** If enabled, points that failed to sample anything will be pruned. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable), AdvancedDisplay)
	bool bPruneFailedSamples = false;
};

struct FPCGExSampleInsidePathContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExSampleInsidePathElement;

	TSharedPtr<PCGExData::FMultiFacadePreloader> TargetsPreloader;

	TArray<TObjectPtr<const UPCGExBlendOpFactory>> BlendingFactories;

	TSharedPtr<PCGExDetails::FDistances> DistanceDetails;

	int32 NumMaxTargets = 0;

	TArray<TSharedRef<PCGExData::FFacade>> TargetFacades;
	TArray<const PCGPointOctree::FPointOctree*> TargetOctrees;
	TSharedPtr<PCGEx::FIndexedItemOctree> TargetsOctree;

	TSharedPtr<PCGExSorting::FPointSorter> Sorter;

	FRuntimeFloatCurve RuntimeWeightCurve;
	const FRichCurve* WeightCurve = nullptr;

	PCGEX_FOREACH_FIELD_InsidePath(PCGEX_OUTPUT_DECL_TOGGLE)

	virtual void RegisterAssetDependencies() override;
};

class FPCGExSampleInsidePathElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(SampleInsidePath)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual void PostLoadAssetsDependencies(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override;
};

namespace PCGExSampleInsidePath
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExSampleInsidePathContext, UPCGExSampleInsidePathSettings>
	{

		TSharedPtr<PCGExPaths::FPath> Path;
		
		TSharedPtr<PCGExDetails::FDistances> DistanceDetails;

		TArray<int8> SamplingMask;

		double RangeMin = 0;
		double RangeMax = 0;

		int8 bAnySuccess = 0;

		TSharedPtr<PCGExDataBlending::FUnionOpsManager> UnionBlendOpsManager;
		TSharedPtr<PCGExDataBlending::IUnionBlender> DataBlender;

		bool bSingleSample = false;
		bool bClosestSample = false;
		bool bOnlySignIfClosed = false;
		bool bOnlyIncrementInsideNumIfClosed = false;

		PCGEX_FOREACH_FIELD_InsidePath(PCGEX_OUTPUT_DECL)

		FBox SampleBox = FBox(ForceInit);

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		void ProcessPath();
		
		void SamplingFailed(const int32 Index);

		virtual void CompleteWork() override;
		virtual void Write() override;

		virtual void Cleanup() override;
	};
}
