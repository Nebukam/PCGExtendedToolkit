// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExWinding.generated.h"

UENUM()
enum class EPCGExWinding : uint8
{
	Clockwise        = 1 UMETA(DisplayName = "Clockwise", ToolTip="Clockwise", ActionIcon="CW"),
	CounterClockwise = 2 UMETA(DisplayName = "Counter Clockwise", ToolTip="Counter Clockwise", ActionIcon="CCW"),
};

UENUM()
enum class EPCGExWindingMutation : uint8
{
	Unchanged        = 0 UMETA(DisplayName = "Unchanged", ToolTip="Unchanged", ActionIcon="Unchanged"),
	Clockwise        = 1 UMETA(DisplayName = "Clockwise", ToolTip="Clockwise", ActionIcon="CW"),
	CounterClockwise = 2 UMETA(DisplayName = "CounterClockwise", ToolTip="Counter Clockwise", ActionIcon="CCW"),
};

namespace PCGExMath
{
	PCGEXCORE_API
	bool IsWinded(const EPCGExWinding Winding, const bool bIsInputClockwise);

	PCGEXCORE_API
	bool IsWinded(const EPCGExWindingMutation Winding, const bool bIsInputClockwise);

	struct PCGEXCORE_API FPolygonInfos
	{
		double Area = 0;
		bool bIsClockwise = false;
		double Perimeter = 0;
		double Compactness = 0;

		FPolygonInfos() = default;
		explicit FPolygonInfos(const TArray<FVector2D>& InPolygon);

		bool IsWinded(const EPCGExWinding Winding) const;
	};

	template <typename T>
	static double AngleCCW(const T& A, const T& B)
	{
		double Angle = FMath::Atan2((A[0] * B[1] - A[1] * B[0]), (A[0] * B[0] + A[1] * B[1]));
		return Angle < 0 ? Angle = TWO_PI + Angle : Angle;
	}
}
