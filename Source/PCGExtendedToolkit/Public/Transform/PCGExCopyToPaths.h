﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"
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

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="transform/copy-to-path"))
class UPCGExCopyToPathsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(CopyToPaths, "Copy to Path", "Deform points along a path/spline.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->ColorTransform); }
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

	/** Which scale components from the sampled transform should be applied to the point.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExApplySampledComponentFlags"))
	uint8 TransformScale = static_cast<uint8>(EPCGExApplySampledComponentFlags::All);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform", meta = (PCG_Overridable))
	bool bPreserveOriginalInputScale = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform", meta = (PCG_Overridable))
	bool bPreserveAspectRatio = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform", meta = (PCG_Overridable))
	EPCGExMinimalAxis FlattenAxis = EPCGExMinimalAxis::None;

#pragma region Main axis

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform", meta = (PCG_Overridable))
	bool bWrapClosedLoops = true;

	// Main axis is "along the spline"

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform", meta = (PCG_Overridable))
	FPCGExAxisDeformDetails MainAxisSettings;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bDoTwist = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform", meta = (PCG_Overridable, EditCondition="bDoTwist"))
	FPCGExAxisDeformDetails TwistSettings = FPCGExAxisDeformDetails(TEXT("StartTwistAmount"), TEXT("EndTwistAmount"), 0, 0);

	/** Used to shrink the scope per-target, to distribute points only on a subselection. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Deform", meta = (PCG_Overridable))
	FPCGExAxisDeformDetails TargetMaskSettings = FPCGExAxisDeformDetails(TEXT("MaskStart"), TEXT("MaskEnd"));

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

	FPCGExAxisDeformDetails MainAxisSettings;
	FPCGExAxisDeformDetails TwistSettings;

	TArray<TSharedPtr<FPCGSplineStruct>> LocalDeformers;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
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

		FTransform AxisTransform = FTransform::Identity;

		TArray<FTransform> Origins;
		TArray<int32> Deformers;
		TArray<TSharedPtr<PCGExData::FPointIO>> Dupes;

		TArray<FPCGExAxisDeformDetails> MainAxisDeformDetails;
		TArray<FPCGExAxisDeformDetails> TwistSettings;

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
