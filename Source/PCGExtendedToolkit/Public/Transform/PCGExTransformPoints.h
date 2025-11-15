// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFitting.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExTransform.h"
#include "Details/PCGExDetailsInputShorthands.h"


#include "PCGExTransformPoints.generated.h"

class FPCGExComputeIOBounds;


UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="transform/move-pivot"))
class UPCGExTransformPointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(TransformPoints, "Transform Points", "A Transform points with the same settings found in Asset Collection variations, with attribute override support.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorTransform; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	virtual PCGExData::EIOInit GetMainDataInitializationPolicy() const override;

public:
#pragma region Translation

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Position", meta = (PCG_Overridable))
	FPCGExInputShorthandNameVector OffsetMin = FPCGExInputShorthandNameVector(FName("OffsetMin"));

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Position", meta = (PCG_Overridable))
	FPCGExInputShorthandNameVector OffsetMax = FPCGExInputShorthandNameVector(FName("OffsetMax"));

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Position", meta = (PCG_Overridable))
	EPCGExVariationSnapping SnapPosition = EPCGExVariationSnapping::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Position", meta = (PCG_Overridable, EditCondition="SnapPosition != EPCGExVariationSnapping::None", EditConditionHides))
	FPCGExInputShorthandNameVector OffsetSnap = FPCGExInputShorthandNameVector(FName("OffsetStep"), FVector(100));

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Position", meta = (PCG_Overridable))
	FPCGExInputShorthandNameBoolean AbsoluteOffset = FPCGExInputShorthandNameBoolean(FName("AbsoluteOffset"), false);
	
#pragma endregion

#pragma region Rotation

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Rotation", meta = (PCG_Overridable))
	FPCGExInputShorthandNameRotator RotationMin = FPCGExInputShorthandNameRotator(FName("RotationMin"));

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Rotation", meta = (PCG_Overridable))
	FPCGExInputShorthandNameRotator RotationMax = FPCGExInputShorthandNameRotator(FName("RotationMax"));

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Rotation", meta = (PCG_Overridable))
	EPCGExVariationSnapping SnapRotation = EPCGExVariationSnapping::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Rotation", meta = (PCG_Overridable, EditCondition="SnapRotation != EPCGExVariationSnapping::None", EditConditionHides))
	FPCGExInputShorthandNameRotator RotationSnap = FPCGExInputShorthandNameRotator(FName("RotationStep"), FRotator(90));

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Rotation", meta=(PCG_NotOverridable, EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExAbsoluteRotationFlags"))
	uint8 AbsoluteRotation = 0;

#pragma endregion

#pragma region Scale

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scale", meta = (PCG_Overridable))
	FPCGExInputShorthandNameVector ScaleMin = FPCGExInputShorthandNameVector(FName("ScaleMin"), FVector::OneVector);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scale", meta = (PCG_Overridable))
	FPCGExInputShorthandNameVector ScaleMax = FPCGExInputShorthandNameVector(FName("ScaleMax"), FVector::OneVector);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scale", meta = (PCG_Overridable))
	EPCGExVariationSnapping SnapScale = EPCGExVariationSnapping::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scale", meta = (PCG_Overridable, EditCondition="SnapScale != EPCGExVariationSnapping::None", EditConditionHides))
	FPCGExInputShorthandNameVector ScaleSnap = FPCGExInputShorthandNameVector(FName("ScaleStep"), FVector(0.1));

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Scale", meta = (PCG_Overridable))
	FPCGExInputShorthandNameBoolean UniformScale = FPCGExInputShorthandNameBoolean(FName("UniformScale"), false);

#pragma endregion

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
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExTransformPoints
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExTransformPointsContext, UPCGExTransformPointsSettings>
	{

		TSharedPtr<PCGExDetails::TSettingValue<FVector>> OffsetMin;
		TSharedPtr<PCGExDetails::TSettingValue<FVector>> OffsetMax;
		TSharedPtr<PCGExDetails::TSettingValue<FVector>> OffsetSnap;
		TSharedPtr<PCGExDetails::TSettingValue<bool>> AbsoluteOffset;
		
		TSharedPtr<PCGExDetails::TSettingValue<FRotator>> RotMin;
		TSharedPtr<PCGExDetails::TSettingValue<FRotator>> RotMax;
		TSharedPtr<PCGExDetails::TSettingValue<FRotator>> RotSnap;
		
		TSharedPtr<PCGExDetails::TSettingValue<FVector>> ScaleMin;
		TSharedPtr<PCGExDetails::TSettingValue<FVector>> ScaleMax;
		TSharedPtr<PCGExDetails::TSettingValue<FVector>> ScaleSnap;
		
	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
	};
}
