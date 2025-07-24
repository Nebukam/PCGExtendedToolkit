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

UCLASS(Hidden, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="transform/copy-to-path"))
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform", meta = (PCG_Overridable))
	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::Center;

#pragma region Main axis

	// Main axis is "along the spline"

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Main Axis", meta = (PCG_Overridable))
	EPCGExCopyToPathsUnit StartUnit = EPCGExCopyToPathsUnit::Alpha;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Main Axis", meta=(PCG_Overridable))
	EPCGExInputValueType StartInput = EPCGExInputValueType::Constant;

	/** Attribute to read start value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Main Axis", meta=(PCG_Overridable, DisplayName="Start (Attr)", EditCondition="StartInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector StartAttribute;

	/** Constant start value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Main Axis", meta=(PCG_Overridable, DisplayName="Start", EditCondition="StartInput == EPCGExInputValueType::Constant", EditConditionHides))
	double Start = 0;

	PCGEX_SETTING_VALUE_GET(Start, double, StartInput, StartAttribute, Start)


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Main Axis", meta = (PCG_Overridable))
	EPCGExCopyToPathsUnit EndUnit = EPCGExCopyToPathsUnit::Alpha;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Main Axis", meta=(PCG_Overridable))
	EPCGExInputValueType EndInput = EPCGExInputValueType::Constant;

	/** Attribute to read end value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Main Axis", meta=(PCG_Overridable, DisplayName="End (Attr)", EditCondition="EndInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector EndAttribute;

	/** Constant end value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform|Main Axis", meta=(PCG_Overridable, DisplayName="End", EditCondition="EndInput == EPCGExInputValueType::Constant", EditConditionHides))
	double End = 0;

	PCGEX_SETTING_VALUE_GET(End, double, EndInput, EndAttribute, End)

#pragma endregion

#pragma region Cross axis

	// Cross axis is "perpendicular to the spline"
	// Controls distance over cross axis direction
	// If bend is enabled, will apply rotation

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
		int32 MainAxis = -1;

		TArray<FTransform> Origins;
		TArray<const FPCGSplineStruct*> Deformers;
		TArray<TSharedPtr<PCGExData::FPointIO>> Dupes;

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
