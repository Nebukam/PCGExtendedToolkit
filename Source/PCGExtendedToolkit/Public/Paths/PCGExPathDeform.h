// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCreateSpline.h"
#include "PCGExGlobalSettings.h"
#include "PCGExPointsProcessor.h"

#include "PCGExPointsProcessor.h"

#include "Tangents/PCGExTangentsInstancedFactory.h"
#include "Transform/PCGExTransform.h"

#include "PCGExPathDeform.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="paths/create-spline"))
class UPCGExPathDeformSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathDeform, "Create Spline", "Create splines from input points.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->NodeColorMiscAdd); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	//~End UPCGExPointsProcessorSettings

	/** Default spline point type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExSplinePointType DefaultPointType = EPCGExSplinePointType::Linear;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bApplyCustomPointType = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "bApplyCustomPointType"))
	FName PointTypeAttribute = "PointType";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExTangentsDetails Tangents;

	bool GetApplyTangents() const
	{
		return (!bApplyCustomPointType && DefaultPointType == EPCGExSplinePointType::CurveCustomTangent);
	}
};

struct FPCGExPathDeformContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExPathDeformElement;
	FPCGExTangentsDetails Tangents;

	TArray<TSharedPtr<PCGExData::FFacade>> PathsFacades;
};

class FPCGExPathDeformElement final : public FPCGExPointsProcessorElement
{
	virtual void DisabledPassThroughData(FPCGContext* Context) const override;

protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(PathDeform)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExPathDeform
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExPathDeformContext, UPCGExPathDeformSettings>
	{
		bool bClosedLoop = false;
		float MaxIndex = 0.0;

		TSharedPtr<PCGExTangents::FTangentsHandler> TangentsHandler;
		TSharedPtr<PCGExData::TBuffer<int32>> CustomPointType;

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
		virtual void Cleanup() override;
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
	};
}
