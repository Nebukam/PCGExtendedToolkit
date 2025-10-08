﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"


#include "Elements/PCGCreateSpline.h"
#include "Tangents/PCGExTangentsInstancedFactory.h"
#include "Transform/PCGExTransform.h"

#include "PCGExCreateSpline.generated.h"

UENUM()
enum class EPCGExSplinePointType : uint8
{
	Linear             = 0 UMETA(DisplayName = "Linear (0)", Tooltip="Linear (0)."),
	Curve              = 1 UMETA(DisplayName = "Curve (1)", Tooltip="Curve (1)."),
	Constant           = 2 UMETA(DisplayName = "Constant (2)", Tooltip="Constant (2)."),
	CurveClamped       = 3 UMETA(DisplayName = "CurveClamped (3)", Tooltip="CurveClamped (3)."),
	CurveCustomTangent = 4 UMETA(DisplayName = "CurveCustomTangent (4)", Tooltip="CurveCustomTangent (4).")
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="paths/create-spline"))
class UPCGExCreateSplineSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	virtual void ApplyDeprecation(UPCGNode* InOutNode) override;

	PCGEX_NODE_INFOS(CreateSpline, "Create Spline", "Create splines from input points.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->ColorMiscAdd); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual FName GetMainOutputPin() const override { return FName(TEXT("Splines")); }
	//~End UPCGExPointsProcessorSettings

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGCreateSplineMode Mode = EPCGCreateSplineMode::CreateDataOnly;

	/** Default spline point type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExSplinePointType DefaultPointType = EPCGExSplinePointType::Linear;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bApplyCustomPointType = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "bApplyCustomPointType"))
	FName PointTypeAttribute = "PointType";

#pragma region DEPRECATED

	UPROPERTY()
	bool bApplyCustomTangents_DEPRECATED = false;

	UPROPERTY()
	FName ArriveTangentAttribute_DEPRECATED = "ArriveTangent";

	UPROPERTY()
	FName LeaveTangentAttribute_DEPRECATED = "LeaveTangent";

#pragma endregion

	/** Per-point tangent settings. Can't be set if the spline is linear. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExTangentsDetails Tangents;

	UPROPERTY(meta = (PCG_Overridable))
	TSoftObjectPtr<AActor> TargetActor;

	/** Specify a list of functions to be called on the target actor after spline mesh creation. Functions need to be parameter-less and with "CallInEditor" flag enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FName> PostProcessFunctionNames;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExAttachmentRules AttachmentRules;

	bool GetApplyTangents() const
	{
		return (!bApplyCustomPointType && DefaultPointType == EPCGExSplinePointType::CurveCustomTangent);
	}
};

struct FPCGExCreateSplineContext final : FPCGExPathProcessorContext
{
	friend class FPCGExCreateSplineElement;
	FPCGExTangentsDetails Tangents;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExCreateSplineElement final : public FPCGExPathProcessorElement
{
	virtual void DisabledPassThroughData(FPCGContext* Context) const override;

protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(CreateSpline)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override;
};

namespace PCGExCreateSpline
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExCreateSplineContext, UPCGExCreateSplineSettings>
	{
		int32 LastIndex = 0;
		bool bClosedLoop = false;
		bool bApplyTangents = false;
		float MaxIndex = 0.0;

		int8 HasAValidEntry = false;

		TSharedPtr<PCGExTangents::FTangentsHandler> TangentsHandler;
		TSharedPtr<PCGExData::TBuffer<int32>> CustomPointType;

		TArray<PCGMetadataEntryKey> SplineEntryKeys;
		TArray<FSplinePoint> SplinePoints;
		FVector PositionOffset = FVector::ZeroVector;

	public:
		TObjectPtr<UPCGSplineData> SplineData = nullptr;
		TObjectPtr<AActor> SplineActor = nullptr;

		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void OnPointsProcessingComplete() override;

		virtual void Output() override;
		virtual void Cleanup() override;
	};

	class FBatch final : public PCGExPointsMT::TBatch<FProcessor>
	{
		AActor* TargetActor = nullptr;
		EPCGCreateSplineMode Mode = EPCGCreateSplineMode::CreateDataOnly;

	public:
		explicit FBatch(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection):
			TBatch(InContext, InPointsCollection)
		{
			PCGEX_TYPED_CONTEXT_AND_SETTINGS(CreateSpline)

			Mode = Settings->Mode;
			TargetActor = Settings->TargetActor.Get() ? Settings->TargetActor.Get() : Context->GetTargetActor(nullptr);
			if (!TargetActor)
			{
				PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Invalid target actor. Ensure TargetActor member is initialized when creating SpatialData."));
				return;
			}

			InContext->AddNotifyActor(TargetActor);
		}

		virtual bool PrepareSingle(const TSharedRef<PCGExPointsMT::IProcessor>& InProcessor) override;
	};
}
