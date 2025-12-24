// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFittingCommon.generated.h"

UENUM()
enum class EPCGExVariationSnapping : uint8
{
	None       = 0 UMETA(DisplayName = "No Snapping", ToolTip="No Snapping", ActionIcon="NoSnapping"),
	SnapOffset = 1 UMETA(DisplayName = "Snap Offset", ToolTip="Snap Offset (the variation value will be snapped, not the result)", ActionIcon="SnapOffset"),
	SnapResult = 2 UMETA(DisplayName = "Snap Result", ToolTip="Snap Result (the variation will not be snapped but the final result will)", ActionIcon="SnapResult"),
};

UENUM(BlueprintType)
enum class EPCGExFitMode : uint8
{
	None       = 0 UMETA(DisplayName = "None", ToolTip="No fitting", ActionIcon="STF_None"),
	Uniform    = 1 UMETA(DisplayName = "Uniform", ToolTip="Uniform fit", ActionIcon="STF_Uniform"),
	Individual = 2 UMETA(DisplayName = "Individual", ToolTip="Per-component fit", ActionIcon="STF_Individual"),
};

UENUM(BlueprintType)
enum class EPCGExScaleToFit : uint8
{
	None = 0 UMETA(DisplayName = "None", ToolTip="No fitting", ActionIcon="Fit_None"),
	Fill = 1 UMETA(DisplayName = "Fill", ToolTip="Fill", ActionIcon="Fit_Fill"),
	Min  = 2 UMETA(DisplayName = "Min", ToolTip="Min", ActionIcon="Fit_Min"),
	Max  = 3 UMETA(DisplayName = "Max", ToolTip="Max", ActionIcon="Fit_Max"),
	Avg  = 4 UMETA(DisplayName = "Average", ToolTip="Average", ActionIcon="Fit_Average"),
};

UENUM(BlueprintType)
enum class EPCGExJustifyFrom : uint8
{
	Min    = 0 UMETA(DisplayName = "Min", ToolTip="Min", ActionIcon="From_Min"),
	Center = 1 UMETA(DisplayName = "Center", ToolTip="Center", ActionIcon="From_Center"),
	Max    = 2 UMETA(DisplayName = "Max", ToolTip="Max", ActionIcon="From_Max"),
	Pivot  = 3 UMETA(DisplayName = "Pivot", ToolTip="Pivot", ActionIcon="From_Pivot"),
	Custom = 4 UMETA(DisplayName = "Custom", ToolTip="Custom", ActionIcon="From_Custom"),
};

UENUM(BlueprintType)
enum class EPCGExJustifyTo : uint8
{
	Min    = 1 UMETA(DisplayName = "Min", ToolTip="Min", ActionIcon="To_Min"),
	Center = 2 UMETA(DisplayName = "Center", ToolTip="Center", ActionIcon="To_Center"),
	Max    = 3 UMETA(DisplayName = "Max", ToolTip="Max", ActionIcon="To_Max"),
	Pivot  = 4 UMETA(DisplayName = "Pivot", ToolTip="Pivot", ActionIcon="To_Pivot"),
	Custom = 5 UMETA(DisplayName = "Custom", ToolTip="Custom", ActionIcon="To_Custom"),
	Same   = 0 UMETA(DisplayName = "Same", ToolTip="Same as 'From'", ActionIcon="To_Same"),
};

UENUM(BlueprintType)
enum class EPCGExVariationMode : uint8
{
	Disabled = 0 UMETA(DisplayName = "Disabled", ToolTip="Disabled", ActionIcon="STF_None"),
	Before   = 1 UMETA(DisplayName = "Before fitting", ToolTip="Pre-processing.\nVariation are applied to the asset before it will be fitted to the host point.", ActionIcon="BeforeStaging"),
	After    = 2 UMETA(DisplayName = "After fitting", ToolTip="Post-processing.\nVariation are applied to the host point after the asset has been fitted inside.", ActionIcon="AfterStaging"),
};
