// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExTransform.h"
#include "Data/PCGExDataForward.h"


#include "PCGExBoundsToPoints.generated.h"

class FPCGExComputeIOBounds;

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class UPCGExBoundsToPointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BoundsToPoints, "Bounds To Points", "Generate a point on the surface of the bounds.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorTransform; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Generates a point collections per generated point */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bGeneratePerPointData = false;

	/** Generate points in symmetry */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExMinimalAxis SymmetryAxis = EPCGExMinimalAxis::None;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExUVW UVW;

	/** The extents of the generate point */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bSetExtents = true;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bSetExtents"))
	FVector Extents = FVector(0.5);

	/** Multiplies the existing bounds */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ As multiplier", EditCondition="bSetExtents", HideEditConditionToggle))
	bool bMultiplyExtents = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bSetScale = true;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bSetScale"))
	FVector Scale = FVector::OneVector;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bGeneratePerPointData"))
	FPCGExAttributeToTagDetails PointAttributesToOutputTags;

private:
	friend class FPCGExBoundsToPointsElement;
};

struct FPCGExBoundsToPointsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExBoundsToPointsElement;
};

class FPCGExBoundsToPointsElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(BoundsToPoints)
	
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExBoundsToPoints
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExBoundsToPointsContext, UPCGExBoundsToPointsSettings>
	{
		int32 NumPoints = 0;
		bool bGeneratePerPointData = false;
		bool bSymmetry = false;

		bool bSetExtents = false;
		FVector Extents = FVector::OneVector;

		bool bSetScale = false;
		FVector Scale = FVector::OneVector;

		EPCGExMinimalAxis Axis = EPCGExMinimalAxis::None;

		FPCGExUVW UVW;
		FPCGExAttributeToTagDetails PointAttributesToOutputTags;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		TArray<TSharedPtr<PCGExData::FPointIO>> NewOutputs;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
