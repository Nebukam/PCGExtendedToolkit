// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExDetailsAxis.generated.h"

UENUM()
enum class EPCGExMinimalAxis : uint8
{
	None = 0 UMETA(DisplayName = "None", ToolTip="None"),
	X    = 1 UMETA(DisplayName = "X", ToolTip="X Axis"),
	Y    = 2 UMETA(DisplayName = "Y", ToolTip="Y Axis"),
	Z    = 3 UMETA(DisplayName = "Z", ToolTip="Z Axis"),
};


UENUM()
enum class EPCGExAxis : uint8
{
	Forward  = 0 UMETA(DisplayName = "Forward", ToolTip="Forward (X+)."),
	Backward = 1 UMETA(DisplayName = "Backward", ToolTip="Backward (X-)."),
	Right    = 2 UMETA(DisplayName = "Right", ToolTip="Right (Y+)"),
	Left     = 3 UMETA(DisplayName = "Left", ToolTip="Left (Y-)"),
	Up       = 4 UMETA(DisplayName = "Up", ToolTip="Up (Z+)"),
	Down     = 5 UMETA(DisplayName = "Down", ToolTip="Down (Z-)"),
};

UENUM(BlueprintType)
enum class EPCGExAxisOrder : uint8
{
	XYZ = 0 UMETA(DisplayName = "X > Y > Z"),
	YZX = 1 UMETA(DisplayName = "Y > Z > X"),
	ZXY = 2 UMETA(DisplayName = "Z > X > Y"),
	YXZ = 3 UMETA(DisplayName = "Y > X > Z"),
	ZYX = 4 UMETA(DisplayName = "Z > Y > X"),
	XZY = 5 UMETA(DisplayName = "X > Z > Y")
};

UENUM()
enum class EPCGExAxisAlign : uint8
{
	Forward  = 0 UMETA(DisplayName = "Forward", ToolTip="..."),
	Backward = 1 UMETA(DisplayName = "Backward", ToolTip="..."),
	Right    = 2 UMETA(DisplayName = "Right", ToolTip="..."),
	Left     = 3 UMETA(DisplayName = "Left", ToolTip="..."),
	Up       = 4 UMETA(DisplayName = "Up", ToolTip="..."),
	Down     = 5 UMETA(DisplayName = "Down", ToolTip="..."),
};

namespace PCGEx
{
	const int32 AxisOrders[6][3] = {
		{0, 1, 2}, // X > Y > Z
		{1, 2, 0}, // Y > Z > X
		{2, 0, 1}, // Z > X > Y
		{1, 0, 2}, // Y > X > Z
		{2, 1, 0}, // Z > Y > X
		{0, 2, 1}  // X > Z > Y
	};

	FORCEINLINE void GetAxisOrder(EPCGExAxisOrder Order, int32 (&OutArray)[3])
	{
		const int32 Index = static_cast<uint8>(Order);
		OutArray[0] = AxisOrders[Index][0];
		OutArray[1] = AxisOrders[Index][1];
		OutArray[2] = AxisOrders[Index][2];
	}

	FORCEINLINE void GetAxisOrder(EPCGExAxisOrder Order, int32& A, int32& B, int32& C)
	{
		const int32 Index = static_cast<uint8>(Order);
		A = AxisOrders[Index][0];
		B = AxisOrders[Index][1];
		C = AxisOrders[Index][2];
	}
}
