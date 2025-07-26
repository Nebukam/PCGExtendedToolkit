// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"
#include "PCGExPointsProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Data/Matching/PCGExMatching.h"
#include "Paths/PCGExCreateSpline.h"
#include "Paths/Tangents/PCGExTangentsInstancedFactory.h"
#include "Sampling/PCGExSampling.h"

#include "Transform/PCGExTransform.h"

#include "PCGExCopyToPaths.generated.h"

UENUM()
enum class EPCGExCopyToPathsUnit : uint8
{
	Alpha    = 0 UMETA(DisplayName = "Alpha", Tooltip="..."),
	Distance = 1 UMETA(DisplayName = "Distance", Tooltip="..."),
};

UENUM()
enum class EPCGExAxisMutation : uint8
{
	None   = 0 UMETA(DisplayName = "None", Tooltip="Keep things as-is"),
	Offset = 1 UMETA(DisplayName = "Offset", Tooltip="Apply an offset along the axis"),
	Bend   = 2 UMETA(DisplayName = "Bend", Tooltip="Bend around the axis")
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="transform/copy-to-path"))
class UPCGExCopyToPathsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(CopyToPaths, "Copy to Path", "Deform points along a path/spline.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->NodeColorTransform); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	//~End UPCGExPointsProcessorSettings

	/** If enabled, allows you to filter out which targets get sampled by which data */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExMatchingDetails DataMatching = FPCGExMatchingDetails(EPCGExMatchingDetailsUsage::Sampling);

	//

	/** Default spline point type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Spline", meta = (PCG_Overridable))
	EPCGExSplinePointType DefaultPointType = EPCGExSplinePointType::Curve;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Spline", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bApplyCustomPointType = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Spline", meta = (PCG_Overridable, EditCondition = "bApplyCustomPointType"))
	FName PointTypeAttribute = "PointType";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Spline", meta = (PCG_Overridable, EditCondition="bApplyCustomPointType || DefaultPointType == EPCGExSplinePointType::CurveCustomTangent"))
	FPCGExTangentsDetails Tangents;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Bounds", meta = (PCG_Overridable))
	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::Center;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Bounds", meta = (PCG_Overridable))
	FVector MinBoundsOffset = FVector::OneVector * -1;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Bounds", meta = (PCG_Overridable))
	FVector MaxBoundsOffset = FVector::OneVector;

	/** Axis transformation order. [Main Axis] > [Cross Axis] > [...]*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform", meta = (PCG_Overridable))
	EPCGExAxisOrder AxisOrder = EPCGExAxisOrder::XYZ;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform", meta = (PCG_Overridable))
	bool bUseScaleForDeformation = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform", meta = (PCG_Overridable))
	bool bPreserveOriginalInputScale = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform", meta = (PCG_Overridable))
	bool bPreserveAspectRatio = false;

#pragma region Main axis

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Main Axis", meta = (PCG_Overridable))
	bool bWrapClosedLoops = true;

	// Main axis is "along the spline"

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Main Axis", meta=(PCG_Overridable))
	EPCGExSampleSource MainAxisStartInput = EPCGExSampleSource::Constant;

	/** Attribute to read start value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Main Axis", meta=(PCG_Overridable, DisplayName="Start (Attr)", EditCondition="MainAxisStartInput != EPCGExSampleSource::Constant", EditConditionHides))
	FName MainAxisStartAttribute = FName("@Data.StartAlpha");;

	/** Constant start value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Main Axis", meta=(PCG_Overridable, DisplayName="Start", EditCondition="MainAxisStartInput == EPCGExSampleSource::Constant", EditConditionHides))
	double MainAxisStart = 0;

	PCGEX_SETTING_DATA_VALUE_GET_BOOL(MainAxisStart, double, MainAxisStartInput != EPCGExSampleSource::Constant, MainAxisStartAttribute, MainAxisStart)

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Main Axis", meta=(PCG_Overridable))
	EPCGExSampleSource MainAxisEndInput = EPCGExSampleSource::Constant;

	/** Attribute to read end value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Main Axis", meta=(PCG_Overridable, DisplayName="End (Attr)", EditCondition="MainAxisEndInput != EPCGExSampleSource::Constant", EditConditionHides))
	FName MainAxisEndAttribute = FName("@Data.EndAlpha");

	/** Constant end value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Main Axis", meta=(PCG_Overridable, DisplayName="End", EditCondition="MainAxisEndInput == EPCGExSampleSource::Constant", EditConditionHides))
	double MainAxisEnd = 1;

	PCGEX_SETTING_DATA_VALUE_GET_BOOL(MainAxisEnd, double, MainAxisEndInput != EPCGExSampleSource::Constant, MainAxisEndAttribute, MainAxisEnd)


#pragma endregion

#pragma region Cross axis

	// Controls distance over cross axis direction
	// If bend is enabled, will apply rotation over that axis
	// it can stretch and offset things in that direction

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Cross Axis", meta = (PCG_Overridable))
	EPCGExAxisMutation CrossAxisMutation = EPCGExAxisMutation::None;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Cross Axis", meta=(PCG_Overridable))
	EPCGExInputValueType CrossAxisStartInput = EPCGExInputValueType::Constant;

	/** Attribute to read start value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Cross Axis", meta=(PCG_Overridable, DisplayName="Start (Attr)", EditCondition="CrossAxisStartInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName CrossAxisStartAttribute = FName("@Data.CrossStart");

	/** Constant start value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Cross Axis", meta=(PCG_Overridable, DisplayName="Start", EditCondition="CrossAxisStartInput == EPCGExInputValueType::Constant", EditConditionHides))
	double CrossAxisStart = 0;

	PCGEX_SETTING_DATA_VALUE_GET(CrossAxisStart, double, CrossAxisStartInput, CrossAxisStartAttribute, CrossAxisStart)

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Cross Axis", meta=(PCG_Overridable))
	EPCGExInputValueType CrossAxisEndInput = EPCGExInputValueType::Constant;

	/** Attribute to read end value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Cross Axis", meta=(PCG_Overridable, DisplayName="End (Attr)", EditCondition="CrossAxisEndInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName CrossAxisEndAttribute = FName("@Data.CrossEnd");

	/** Constant end value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Cross Axis", meta=(PCG_Overridable, DisplayName="End", EditCondition="CrossAxisEndInput == EPCGExInputValueType::Constant", EditConditionHides))
	double CrossAxisEnd = 1;

	PCGEX_SETTING_DATA_VALUE_GET(CrossAxisEnd, double, CrossAxisEndInput, CrossAxisEndAttribute, CrossAxisEnd)

#pragma endregion

	bool GetApplyTangents() const
	{
		return (!bApplyCustomPointType && DefaultPointType == EPCGExSplinePointType::CurveCustomTangent);
	}
};

struct FPCGExCopyToPathsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExCopyToPathsElement;
	FPCGExTangentsDetails Tangents;

	bool bUseUnifiedBounds = false;
	FBox UnifiedBounds = FBox(ForceInit);

	TSharedPtr<PCGExMatching::FDataMatcher> DataMatcher;

	TArray<PCGExData::FTaggedData> DeformersData;
	TArray<TSharedPtr<PCGExData::FFacade>> DeformersFacades;
	TArray<const FPCGSplineStruct*> Deformers;

	TArray<TSharedPtr<PCGExDetails::TSettingValue<double>>> MainAxisStart;
	TArray<TSharedPtr<PCGExDetails::TSettingValue<double>>> MainAxisEnd;

	TArray<TSharedPtr<FPCGSplineStruct>> LocalDeformers;
};

class FPCGExCopyToPathsElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(CopyToPaths)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExCopyToPaths
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExCopyToPathsContext, UPCGExCopyToPathsSettings>
	{
		FBox Box = FBox(ForceInit);
		FVector Size = FVector::ZeroVector;

		FTransform FwdT = FTransform::Identity;

		TArray<FTransform> Origins;
		TArray<int32> Deformers;
		TArray<TSharedPtr<PCGExData::FPointIO>> Dupes;

		TSharedPtr<PCGExDetails::TSettingValue<double>> MainAxisStart;
		TSharedPtr<PCGExDetails::TSettingValue<double>> MainAxisEnd;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void CompleteWork() override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
	};

	class FBatch final : public PCGExPointsMT::TBatch<FProcessor>
	{
		AActor* TargetActor = nullptr;

	public:
		explicit FBatch(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection):
			TBatch(InContext, InPointsCollection)
		{
		}

		virtual void OnInitialPostProcess() override;
		void BuildSpline(const int32 InSplineIndex) const;
		void OnSplineBuildingComplete();
	};
}
