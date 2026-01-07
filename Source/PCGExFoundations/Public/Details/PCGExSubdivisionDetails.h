// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExDataCommon.h"
#include "Details/PCGExSettingsMacros.h"
#include "Math/PCGExMathAxis.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "PCGExSubdivisionDetails.generated.h"

struct FPCGExContext;

namespace PCGExData
{
	class FFacade;
}

namespace PCGExDetails
{
	template <typename T>
	class TSettingValue;
}

UENUM()
enum class EPCGExSubdivideMode : uint8
{
	Distance  = 0 UMETA(DisplayName = "Distance", ToolTip="Number of subdivisions depends on length"),
	Count     = 1 UMETA(DisplayName = "Count", ToolTip="Number of subdivisions is fixed"),
	Manhattan = 2 UMETA(DisplayName = "Manhattan", ToolTip="Manhattan subdivision, number of subdivisions depends on spatial relationship between the points; will be in the [0..2] range."),
};

UENUM()
enum class EPCGExManhattanMethod : uint8
{
	Simple       = 0 UMETA(DisplayName = "Simple", ToolTip="Simple Manhattan subdivision, will generate 0..2 points"),
	GridDistance = 1 UMETA(DisplayName = "Grid (Distance)", ToolTip="Grid Manhattan subdivision, will subdivide space according to a grid size."),
	GridCount    = 2 UMETA(DisplayName = "Grid (Count)", ToolTip="Grid Manhattan subdivision, will subdivide space according to a grid size."),
};

UENUM()
enum class EPCGExManhattanAlign : uint8
{
	World    = 0 UMETA(DisplayName = "World", ToolTip=""),
	Custom   = 1 UMETA(DisplayName = "Custom", ToolTip=""),
	SegmentX = 5 UMETA(DisplayName = "Segment X", ToolTip=""),
	SegmentY = 6 UMETA(DisplayName = "Segment Y", ToolTip=""),
	SegmentZ = 7 UMETA(DisplayName = "Segment Z", ToolTip=""),
};

USTRUCT(BlueprintType)
struct PCGEXFOUNDATIONS_API FPCGExManhattanDetails
{
	GENERATED_BODY()

	explicit FPCGExManhattanDetails(const bool InSupportAttribute = false)
		: bSupportAttribute(InSupportAttribute)
	{
	}

	UPROPERTY()
	bool bSupportAttribute = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExManhattanMethod Method = EPCGExManhattanMethod::Simple;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExAxisOrder Order = EPCGExAxisOrder::XYZ;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="bSupportAttribute", EditConditionHides))
	EPCGExInputValueType GridSizeInput = EPCGExInputValueType::Constant;

	/** Max Length Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName="Grid Size (Attr)", EditCondition="bSupportAttribute && Method != EPCGExManhattanMethod::Simple && GridSizeInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName GridSizeAttribute = FName("GridSize");

	/** Grid Size Constant -- If using count, values will be rounded down to the nearest int. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Grid Size", EditCondition="Method != EPCGExManhattanMethod::Simple && (!bSupportAttribute || GridSizeInput == EPCGExInputValueType::Constant)", EditConditionHides))
	FVector GridSize = FVector(10);

	PCGEX_SETTING_VALUE_DECL(GridSize, FVector)

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExManhattanAlign SpaceAlign = EPCGExManhattanAlign::World;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bSupportAttribute && SpaceAlign == EPCGExManhattanAlign::Custom", EditConditionHides))
	EPCGExInputValueType OrientInput = EPCGExInputValueType::Constant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Orient (Attr)", EditCondition="bSupportAttribute && OrientInput != EPCGExInputValueType::Constant && SpaceAlign == EPCGExManhattanAlign::Custom", EditConditionHides))
	FPCGAttributePropertyInputSelector OrientAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Orient", ClampMin=0, EditCondition="SpaceAlign == EPCGExManhattanAlign::Custom && (!bSupportAttribute || OrientInput == EPCGExInputValueType::Constant)", EditConditionHides))
	FQuat OrientConstant = FQuat::Identity;

	PCGEX_SETTING_VALUE_DECL(Orient, FQuat)

	bool IsValid() const;
	bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade);
	int32 ComputeSubdivisions(const FVector& A, const FVector& B, const int32 Index, TArray<FVector>& OutSubdivisions, double& OutDist) const;

protected:
	bool bInitialized = false;

	int32 Comps[3] = {0, 0, 0};
	TSharedPtr<PCGExDetails::TSettingValue<FVector>> GridSizeBuffer;
	TSharedPtr<PCGExDetails::TSettingValue<FQuat>> OrientBuffer;
};
