// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Elements/PCGCreateSpline.h"
#include "Tangents/PCGExTangentsOperation.h"

#include "PCGExCreateSpline.generated.h"

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Spline Point Type")--E*/)
enum class EPCGExSplinePointType : uint8
{
	Linear             = 0 UMETA(DisplayName = "Linear (0)", Tooltip="Linear (0)."),
	Curve              = 1 UMETA(DisplayName = "Curve (1)", Tooltip="Curve (1)."),
	Constant           = 2 UMETA(DisplayName = "Constant (2)", Tooltip="Constant (2)."),
	CurveClamped       = 3 UMETA(DisplayName = "CurveClamped (3)", Tooltip="CurveClamped (3)."),
	CurveCustomTangent = 4 UMETA(DisplayName = "CurveCustomTangent (4)", Tooltip="CurveCustomTangent (4).")
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExCreateSplineSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(CreateSpline, "Create Spline", "Create splines from input points.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscAdd; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual FName GetMainOutputLabel() const override { return FName(TEXT("Splines")); }
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
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

	/** Allow to specify custom tangents for each point, as an attribute. Can't be set if the spline is linear. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bApplyCustomTangents = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "bApplyCustomTangents", EditConditionHides))
	FName ArriveTangentAttribute = "ArriveTangent";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "bApplyCustomTangents", EditConditionHides))
	FName LeaveTangentAttribute = "LeaveTangent";

	UPROPERTY(meta = (PCG_Overridable))
	TSoftObjectPtr<AActor> TargetActor;

	/** Specify a list of functions to be called on the target actor after spline mesh creation. Functions need to be parameter-less and with "CallInEditor" flag enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FName> PostProcessFunctionNames;

	bool GetApplyTangents() const
	{
		return (!bApplyCustomPointType && DefaultPointType == EPCGExSplinePointType::CurveCustomTangent);
	}
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCreateSplineContext final : FPCGExPathProcessorContext
{
	friend class FPCGExCreateSplineElement;

	TSet<AActor*> NotifyActors;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCreateSplineElement final : public FPCGExPathProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

	virtual void DisabledPassThroughData(FPCGContext* Context) const override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override;
};

namespace PCGExCreateSpline
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExCreateSplineContext, UPCGExCreateSplineSettings>
	{
		int32 LastIndex = 0;
		bool bClosedLoop = false;
		bool bApplyTangents = false;
		float MaxIndex = 0.0;

		TSharedPtr<PCGExData::TBuffer<FVector>> ArriveTangent;
		TSharedPtr<PCGExData::TBuffer<FVector>> LeaveTangent;

		TSharedPtr<PCGExData::TBuffer<int32>> CustomPointType;

		TArray<FSplinePoint> SplinePoints;
		FVector PositionOffset = FVector::ZeroVector;

	public:
		TObjectPtr<UPCGSplineData> SplineData = nullptr;
		TObjectPtr<AActor> SplineActor = nullptr;

		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
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
		}

		virtual bool PrepareSingle(const TSharedPtr<FProcessor>& PointsProcessor) override;
	};
}
