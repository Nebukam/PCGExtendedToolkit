// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExNoise3DCommon.generated.h"

/**
 * Blend mode for combining multiple noise operations
 */
UENUM(BlueprintType)
enum class EPCGExNoiseBlendMode : uint8
{
	Blend UMETA(DisplayName = "Blend (Weighted Average)", ToolTip="Weighted average of all noise values"),
	Add UMETA(DisplayName = "Add", ToolTip="Add all noise values (clamped)"),
	Multiply UMETA(DisplayName = "Multiply", ToolTip="Multiply all noise values"),
	Min UMETA(DisplayName = "Min", ToolTip="Take minimum value"),
	Max UMETA(DisplayName = "Max", ToolTip="Take maximum value"),
	Subtract UMETA(DisplayName = "Subtract", ToolTip="Subtract subsequent values from first"),
	Screen UMETA(DisplayName = "Screen", ToolTip="Screen blend (1 - (1-a)*(1-b))"),
	Overlay UMETA(DisplayName = "Overlay", ToolTip="Overlay blend"),
	SoftLight UMETA(DisplayName = "Soft Light", ToolTip="Soft light blend"),
	First UMETA(DisplayName = "First Valid", ToolTip="Take the first non-zero value"),
};


namespace PCGExNoise3D
{
	namespace Labels
	{
		const FName OutputNoise3DLabel = FName("Noise");
		const FName SourceNoise3DLabel = FName("Noises");
	}
}
