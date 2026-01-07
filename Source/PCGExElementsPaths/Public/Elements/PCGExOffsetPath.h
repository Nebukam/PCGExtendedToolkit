// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFilterCommon.h"
#include "Factories/PCGExFactories.h"
#include "Core/PCGExPathProcessor.h"
#include "Details/PCGExSettingsMacros.h"
#include "Paths/PCGExPath.h"
#include "Paths/PCGExPathIntersectionDetails.h"
#include "Paths/PCGExPathsCommon.h"

#include "PCGExOffsetPath.generated.h"

namespace PCGExPaths
{
	class FPathEdgeHalfAngle;
	class FPath;
	struct FPathEdgeCrossings;
}

UENUM()
enum class EPCGExOffsetCleanupMode : uint8
{
	None            = 0 UMETA(DisplayName = "None", ToolTip="No cleanup."),
	CollapseFlipped = 1 UMETA(DisplayName = "Collapse Flipped Segments", ToolTip="Collapse flipped segments."),
	SectionsFlipped = 2 UMETA(DisplayName = "Collapse Sections (Flipped)", ToolTip="Remove sections of the paths that self-intersect if that section contains flipped segments."),
	Sections        = 3 UMETA(DisplayName = "Collapse Sections", ToolTip="Remove sections of the paths that are between self-intersections."),
};

UENUM()
enum class EPCGExOffsetAdjustment : uint8
{
	None         = 0 UMETA(DisplayName = "Raw", ToolTip="..."),
	SmoothCustom = 1 UMETA(DisplayName = "Custom Smooth", ToolTip="..."),
	SmoothAuto   = 2 UMETA(DisplayName = "Auto Smooth", ToolTip="..."),
	Mitre        = 3 UMETA(DisplayName = "Mitre", ToolTip="..."),
};

UENUM()
enum class EPCGExOffsetMethod : uint8
{
	Slide     = 0 UMETA(DisplayName = "Slide", ToolTip="..."),
	LinePlane = 1 UMETA(DisplayName = "Line/Plane", ToolTip="..."),
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/offset"))
class UPCGExOffsetPathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathOffset, "Path : Offset", "Offset paths points.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	virtual PCGExData::EIOInit GetMainDataInitializationPolicy() const override;

	//~Begin UPCGExPointsProcessorSettings
public:
	PCGEX_NODE_POINT_FILTER(PCGExFilters::Labels::SourceFiltersLabel, "Filters which points will be offset", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExOffsetMethod OffsetMethod = EPCGExOffsetMethod::Slide;

	/** Offset type.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExInputValueType OffsetInput = EPCGExInputValueType::Constant;

	/** Fetch the offset size from a local attribute. The regular Size parameter then act as a scale.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Offset (Attr)", EditCondition="OffsetInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector OffsetAttribute;

	/** Offset size.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Offset", EditCondition="OffsetInput == EPCGExInputValueType::Constant", EditConditionHides))
	double OffsetConstant = 1.0;

	PCGEX_SETTING_VALUE_DECL(Offset, double)

	/** Scale offset direction & distance using point scale.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bApplyPointScaleToOffset = false;

	/** Up vector used to calculate Offset direction.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FVector UpVectorConstant = PCGEX_CORE_SETTINGS.WorldUp;

	/** Direction Vector type.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExInputValueType DirectionType = EPCGExInputValueType::Constant;

	/** Fetch the direction vector from a local point attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Direction (Attr)", EditCondition="DirectionType != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector DirectionAttribute;

	/** Type of arithmetic path point offset direction.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Direction", EditCondition="OffsetMethod == EPCGExOffsetMethod::Slide && DirectionType == EPCGExInputValueType::Constant", EditConditionHides))
	EPCGExPathNormalDirection DirectionConstant = EPCGExPathNormalDirection::AverageNormal;

	/** Inverts offset direction. Can also be achieved by using negative offset values, but this enable consistent inversion no matter the input.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bInvertDirection = false;


	/** Adjust aspect in tight angles */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="OffsetMethod == EPCGExOffsetMethod::Slide"))
	EPCGExOffsetAdjustment Adjustment = EPCGExOffsetAdjustment::SmoothAuto;

	/** Adjust aspect in tight angles */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="OffsetMethod == EPCGExOffsetMethod::Slide && Adjustment == EPCGExOffsetAdjustment::SmoothCustom", EditConditionHides))
	double AdjustmentScale = -0.5;

	/** Offset size.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="OffsetMethod == EPCGExOffsetMethod::Slide && Adjustment == EPCGExOffsetAdjustment::Mitre", EditConditionHides))
	double MitreLimit = 4.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Cleanup", meta = (PCG_NotOverridable))
	EPCGExOffsetCleanupMode CleanupMode = EPCGExOffsetCleanupMode::None;

	/** During cleanup, used as a tolerance to consider valid path segments as overlapping or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Cleanup", meta = (PCG_Overridable, EditCondition="CleanupMode != EPCGExOffsetCleanupMode::None", EditConditionHides))
	double IntersectionTolerance = 1;

	/** Attempt to adjust offset on mutated edges .*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Cleanup", meta = (PCG_Overridable, EditCondition="CleanupMode != EPCGExOffsetCleanupMode::None", EditConditionHides))
	bool bFlagMutatedPoints = false;

	/** Name of the 'bool' attribute to flag the nodes that are the result of a mutation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Cleanup", meta=(PCG_Overridable, EditCondition="CleanupMode != EPCGExOffsetCleanupMode::None && bFlagMutatedPoints", EditConditionHides))
	FName MutatedAttributeName = FName("IsMutated");

	/** Whether to flag points that have been flipped during the offset.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Cleanup", meta = (PCG_Overridable, EditCondition="CleanupMode == EPCGExOffsetCleanupMode::None"))
	bool bFlagFlippedPoints = false;

	/** Name of the 'bool' attribute to flag the points that are flipped. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Cleanup", meta=(PCG_Overridable, EditCondition="CleanupMode == EPCGExOffsetCleanupMode::None && bFlagFlippedPoints"))
	FName FlippedAttributeName = FName("IsFlipped");
};

struct FPCGExOffsetPathContext final : FPCGExPathProcessorContext
{
	friend class FPCGExOffsetPathElement;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExOffsetPathElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(OffsetPath)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExOffsetPath
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExOffsetPathContext, UPCGExOffsetPathSettings>
	{
		TConstPCGValueRange<FTransform> InTransforms;

		FPCGExPathEdgeIntersectionDetails CrossingSettings;

		TSharedPtr<PCGExPaths::FPath> Path;
		TSharedPtr<PCGExPaths::FPathEdgeHalfAngle> PathAngles;
		TSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>> OffsetDirection;

		TBitArray<> CleanEdge;
		TArray<TSharedPtr<PCGExPaths::FPathEdgeCrossings>> EdgeCrossings;

		int32 FirstFlippedEdge = -1;
		TSharedPtr<PCGExPaths::FPath> DirtyPath;
		TSharedPtr<PCGExPaths::FPathEdgeLength> DirtyLength;
		TBitArray<> Mutated;

		double DirectionFactor = -1; // Default to -1 because the normal maths changed at some point, inverting all existing value. Sorry for the lack of elegance.
		double OffsetConstant = 0;
		FVector Up = FVector::UpVector;

		TSharedPtr<PCGExDetails::TSettingValue<double>> OffsetGetter;
		TSharedPtr<PCGExData::TBuffer<FVector>> DirectionGetter;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;

		virtual void OnPointsProcessingComplete() override;
		virtual void CompleteWork() override;

		int32 CollapseFrom(const int32 StartIndex, TArray<int32>& KeptPoints, const bool bFlippedOnly);
		void CollapseSections(const bool bFlippedOnly);

		void MarkMutated();
	};
}
