﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExTransform.h"
#include "Data/Matching/PCGExMatching.h"
#include "Details/PCGExDetailsDistances.h"
#include "Details/PCGExSettingsMacros.h"


#include "PCGExBestMatchAxis.generated.h"

class FPCGExComputeIOBounds;

UENUM()
enum class EPCGExBestMatchAxisTargetMode : uint8
{
	Direction              = 0 UMETA(DisplayName = "Direction", ToolTip="Best match against a direction vector."),
	LookAtWorldPosition    = 1 UMETA(DisplayName = "Look at Position (World)", ToolTip="Best match against the look at vector toward a world position."),
	LookAtRelativePosition = 2 UMETA(DisplayName = "Look at Position (Relative)", ToolTip="Best match against the look at vector toward a relative position."),
	ClosestTarget          = 3 UMETA(DisplayName = "Look at Closest Target", ToolTip="Best match against the look at vector toward the closest target point.")
};

UCLASS(Hidden, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="transform/move-pivot"))
class UPCGExBestMatchAxisSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BestMatchAxis, "Best Match Axis", "Rotate a point or transform to closely match an input direction (or look at location) but preserve orthogonality.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorTransform; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	virtual  PCGExData::EIOInit GetMainDataInitializationPolicy() const override;
	
public:
	/** Drive the best match axis */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExBestMatchAxisTargetMode Mode = EPCGExBestMatchAxisTargetMode::Direction;


	/** Up vector source.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode != EPCGExBestMatchAxisTargetMode::ClosestTarget", EditConditionHides))
	EPCGExInputValueType MatchInput = EPCGExInputValueType::Attribute;

	/** The attribute or property on selected source to use as Up vector for the look at transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Match (Attr)", EditCondition="Mode != EPCGExBestMatchAxisTargetMode::ClosestTarget && MatchInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector MatchSource;

	/** The constant to use as Up vector for the look at transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Match", EditCondition="Mode != EPCGExBestMatchAxisTargetMode::ClosestTarget && MatchInput == EPCGExInputValueType::Constant", EditConditionHides))
	FVector MatchConstant = FVector::UpVector;

	PCGEX_SETTING_VALUE_DECL(Match, FVector)

	// TODO : Support attribute mutation such as transform, rotator, vector
	// TODO : Auto-pick axis based on unsigned dot product (so we only mutate where it make the most meaingful)

	/** If enabled, allows you to filter out which targets get sampled by which data */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode == EPCGExBestMatchAxisTargetMode::ClosestTarget", EditConditionHides))
	FPCGExMatchingDetails DataMatching = FPCGExMatchingDetails(EPCGExMatchingDetailsUsage::Sampling);

	/** Distance method to be used for source & target points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode == EPCGExBestMatchAxisTargetMode::ClosestTarget", EditConditionHides))
	FPCGExDistanceDetails DistanceDetails;

private:
	friend class FPCGExBestMatchAxisElement;
};

struct FPCGExBestMatchAxisContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExBestMatchAxisElement;

	TSharedPtr<PCGExSampling::FTargetsHandler> TargetsHandler;
	int32 NumMaxTargets = 0;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExBestMatchAxisElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(BestMatchAxis)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExBestMatchAxis
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExBestMatchAxisContext, UPCGExBestMatchAxisSettings>
	{
		TSet<const UPCGData*> IgnoreList;
		TSharedPtr<PCGExDetails::TSettingValue<FVector>> MatchGetter;

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
