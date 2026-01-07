// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFilterCommon.h"
#include "Factories/PCGExFactories.h"


#include "Core/PCGExPointsProcessor.h"
#include "Details/PCGExInputShorthandsDetails.h"
#include "Fitting/PCGExFittingCommon.h"


#include "PCGExTransformPoints.generated.h"

class FPCGExComputeIOBounds;


UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="transform/transform-points"))
class UPCGExTransformPointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(TransformPoints, "Transform Points", "A Transform points with the same settings found in Asset Collection variations, with attribute override support.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::PointOps; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(Transform); }
#endif

	PCGEX_NODE_POINT_FILTER(PCGExFilters::Labels::SourceFiltersLabel, "Filters", PCGExFactories::PointFilters, false)

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	virtual PCGExData::EIOInit GetMainDataInitializationPolicy() const override;

public:
#pragma region Translation

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Position", meta = (PCG_Overridable))
	FPCGExInputShorthandSelectorVector OffsetMin = FPCGExInputShorthandSelectorVector(FName("OffsetMin"));

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Position", meta = (PCG_Overridable))
	FPCGExInputShorthandSelectorVector OffsetMax = FPCGExInputShorthandSelectorVector(FName("OffsetMax"));

	/** Scale applied to both Offset Min & Offset Max */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Position", meta = (PCG_Overridable, DisplayName=" └─ Scaling"))
	FPCGExInputShorthandSelectorVector OffsetScaling = FPCGExInputShorthandSelectorVector(FName("Scaling"), FVector(1));

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Position", meta = (PCG_Overridable))
	EPCGExVariationSnapping SnapPosition = EPCGExVariationSnapping::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Position", meta = (PCG_Overridable, EditCondition="SnapPosition != EPCGExVariationSnapping::None", EditConditionHides))
	FPCGExInputShorthandSelectorVector OffsetSnap = FPCGExInputShorthandSelectorVector(FName("OffsetStep"), FVector(100));

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Position", meta = (PCG_Overridable))
	FPCGExInputShorthandSelectorBoolean AbsoluteOffset = FPCGExInputShorthandSelectorBoolean(FName("AbsoluteOffset"), false, false);

#pragma endregion

#pragma region Rotation

	/** If enabled will first reset rotation to 0, then apply variation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Rotation", meta = (PCG_Overridable))
	bool bResetRotation = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Rotation", meta = (PCG_Overridable))
	FPCGExInputShorthandSelectorRotator RotationMin = FPCGExInputShorthandSelectorRotator(FName("RotationMin"));

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Rotation", meta = (PCG_Overridable))
	FPCGExInputShorthandSelectorRotator RotationMax = FPCGExInputShorthandSelectorRotator(FName("RotationMax"));

	/** Scale applied to both Rotation Min & Rotation Max */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Rotation", meta = (PCG_Overridable, DisplayName=" └─ Scaling"))
	FPCGExInputShorthandSelectorVector RotationScaling = FPCGExInputShorthandSelectorVector(FName("Scaling"), FVector(1));

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Rotation", meta = (PCG_Overridable))
	EPCGExVariationSnapping SnapRotation = EPCGExVariationSnapping::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Rotation", meta = (PCG_Overridable, EditCondition="SnapRotation != EPCGExVariationSnapping::None", EditConditionHides))
	FPCGExInputShorthandSelectorRotator RotationSnap = FPCGExInputShorthandSelectorRotator(FName("RotationStep"), FRotator(90));

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Rotation", meta=(PCG_NotOverridable, EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExCore.EPCGExAbsoluteRotationFlags"))
	uint8 AbsoluteRotation = 0;

#pragma endregion

#pragma region Scale

	/** If enabled will first reset scale to 1, then apply variation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scale", meta = (PCG_Overridable))
	bool bResetScale = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scale", meta = (PCG_Overridable))
	FPCGExInputShorthandSelectorVector ScaleMin = FPCGExInputShorthandSelectorVector(FName("ScaleMin"), FVector::OneVector);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scale", meta = (PCG_Overridable))
	FPCGExInputShorthandSelectorVector ScaleMax = FPCGExInputShorthandSelectorVector(FName("ScaleMax"), FVector::OneVector);

	/** Scale applied to both Scale Min & Scale Max */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scale", meta = (PCG_Overridable, DisplayName=" └─ Scaling"))
	FPCGExInputShorthandSelectorVector ScaleScaling = FPCGExInputShorthandSelectorVector(FName("Scaling"), FVector(1));

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scale", meta = (PCG_Overridable))
	FPCGExInputShorthandSelectorBoolean UniformScale = FPCGExInputShorthandSelectorBoolean(FName("UniformScale"), false, false);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scale", meta = (PCG_Overridable))
	EPCGExVariationSnapping SnapScale = EPCGExVariationSnapping::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scale", meta = (PCG_Overridable, EditCondition="SnapScale != EPCGExVariationSnapping::None", EditConditionHides))
	FPCGExInputShorthandSelectorVector ScaleSnap = FPCGExInputShorthandSelectorVector(FName("ScaleStep"), FVector(0.1));

#pragma endregion

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extras", meta=(PCG_Overridable))
	bool bApplyScaleToBounds = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extras", meta=(PCG_Overridable))
	bool bResetPointCenter = false;

	/** Bounds-relative coordinate used for the new center */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extras", meta = (PCG_Overridable, DisplayName=" └─ Point Center Location", EditCondition="bResetPointCenter", EditConditionHides))
	FPCGExInputShorthandSelectorVector PointCenterLocation = FPCGExInputShorthandSelectorVector(FName("PointCenter"), FVector(0.5));

private:
	friend class FPCGExTransformPointsElement;
};

struct FPCGExTransformPointsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExTransformPointsElement;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExTransformPointsElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(TransformPoints)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExTransformPoints
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExTransformPointsContext, UPCGExTransformPointsSettings>
	{
		TSharedPtr<PCGExDetails::TSettingValue<FVector>> OffsetMin;
		TSharedPtr<PCGExDetails::TSettingValue<FVector>> OffsetMax;
		TSharedPtr<PCGExDetails::TSettingValue<FVector>> OffsetScale;
		TSharedPtr<PCGExDetails::TSettingValue<FVector>> OffsetSnap;
		TSharedPtr<PCGExDetails::TSettingValue<bool>> AbsoluteOffset;

		TSharedPtr<PCGExDetails::TSettingValue<FRotator>> RotMin;
		TSharedPtr<PCGExDetails::TSettingValue<FRotator>> RotMax;
		TSharedPtr<PCGExDetails::TSettingValue<FVector>> RotScale;
		TSharedPtr<PCGExDetails::TSettingValue<FRotator>> RotSnap;

		TSharedPtr<PCGExDetails::TSettingValue<FVector>> ScaleMin;
		TSharedPtr<PCGExDetails::TSettingValue<FVector>> ScaleMax;
		TSharedPtr<PCGExDetails::TSettingValue<FVector>> ScaleScale;
		TSharedPtr<PCGExDetails::TSettingValue<FVector>> ScaleSnap;
		TSharedPtr<PCGExDetails::TSettingValue<bool>> UniformScale;

		TSharedPtr<PCGExDetails::TSettingValue<FVector>> PointCenter;

		bool bApplyScaleToBounds = false;
		bool bResetPointCenter = false;
		bool bAllocatedBounds = false;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
	};
}
