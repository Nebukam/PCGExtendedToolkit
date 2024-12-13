// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExTransform.h"
#include "Data/PCGExDataForward.h"

#include "PCGExBoundsAxisToPoints.generated.h"

class FPCGExComputeIOBounds;

UENUM()
enum class EPCGExBoundAxisPriority : uint8
{
	Shortest = 0 UMETA(DisplayName = "Shortest", ToolTip="Shortest axis"),
	Longest  = 1 UMETA(DisplayName = "Longest", ToolTip="Longest axis"),
	Median   = 2 UMETA(DisplayName = "Median", ToolTip="The leftover axis, that is neither the shortest nor the longest.")
};

UENUM()
enum class EPCGExAxisDirectionConstraint : uint8
{
	None  = 0 UMETA(DisplayName = "None", ToolTip="..."),
	Avoid = 1 UMETA(DisplayName = "Avoid", ToolTip="..."),
	Favor = 2 UMETA(DisplayName = "Favor", ToolTip="..."),
};

UENUM()
enum class EPCGExAxisSizeConstraint : uint8
{
	None    = 0 UMETA(DisplayName = "None", ToolTip="..."),
	Greater = 1 UMETA(DisplayName = "Greater", ToolTip="..."),
	Smaller = 1 UMETA(DisplayName = "Smaller", ToolTip="..."),
};

UENUM()
enum class EPCGExAxisConstraintSorting : uint8
{
	SizeMatters      = 0 UMETA(DisplayName = "Size matters more", ToolTip="..."),
	DirectionMatters = 1 UMETA(DisplayName = "Direction matters more", ToolTip="..."),
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBoundsAxisToPointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BoundsAxisToPoints, "Bounds Axis To Points", "Generate a two-point from a bound axis.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorTransform; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Generates a point collections per generated point */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bGeneratePerPointData = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsReference = EPCGExPointBoundsSource::ScaledBounds;

	/** Which initial direction should initially picked. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExBoundAxisPriority Priority = EPCGExBoundAxisPriority::Shortest;

	/** Shifts the axis selection based on whether the selected axis points toward or away from a static direction. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExAxisDirectionConstraint DirectionConstraint = EPCGExAxisDirectionConstraint::Avoid;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Direction", EditCondition="DirectionConstraint != EPCGExAxisDirectionConstraint::None", EditConditionHides))
	FVector Direction = FVector::UpVector;

	/** Shifts the axis selection based on whether its size is greater or smaller than a given threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExAxisSizeConstraint SizeConstraint = EPCGExAxisSizeConstraint::Greater;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Threshold", EditCondition="SizeConstraint != EPCGExAxisSizeConstraint::None", EditConditionHides))
	double SizeThreshold = 0.1;

	/** In which order shifting should be processed, as one is likely to override the other. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="DirectionConstraint != EPCGExAxisDirectionConstraint::None && SizeConstraint != EPCGExAxisSizeConstraint::None", EditConditionHides))
	EPCGExAxisConstraintSorting ConstraintsOrder = EPCGExAxisConstraintSorting::DirectionMatters;

	/** Extent factor at which the points will be created on the selected world-align axis */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double U = 1;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bSetExtents = true;

	/** Set the output point' extent to this value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bSetExtents"))
	FVector Extents = FVector(0.5);

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bSetScale = true;

	/**  Set the output point' scale to this value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bSetScale"))
	FVector Scale = FVector::OneVector;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bGeneratePerPointData"))
	FPCGExAttributeToTagDetails PointAttributesToOutputTags;

private:
	friend class FPCGExBoundsAxisToPointsElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBoundsAxisToPointsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExBoundsAxisToPointsElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBoundsAxisToPointsElement final : public FPCGExPointsProcessorElement
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExBoundsAxisToPoints
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExBoundsAxisToPointsContext, UPCGExBoundsAxisToPointsSettings>
	{
		int32 NumPoints = 0;
		bool bGeneratePerPointData = false;

		bool bSetExtents = false;
		FVector Extents = FVector::OneVector;

		bool bSetScale = false;
		FVector Scale = FVector::OneVector;

		FPCGExAttributeToTagDetails PointAttributesToOutputTags;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		TArray<TSharedPtr<PCGExData::FPointIO>> NewOutputs;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
