// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGExMathMean.generated.h"

UENUM(BlueprintType)
enum class EPCGExMeanMeasure : uint8
{
	Relative = 0 UMETA(DisplayName = "Relative", ToolTip="Input value will be normalized between 0..1, or used as a factor. (what it means exactly depends on context. See node-specific documentation.)"),
	Discrete = 1 UMETA(DisplayName = "Discrete", ToolTip="Raw value will be used, or used as absolute. (what it means exactly depends on context. See node-specific documentation.)"),
};

UENUM(BlueprintType)
enum class EPCGExMeanMethod : uint8
{
	Average = 0 UMETA(DisplayName = "Average", ToolTip="Average"),
	Median  = 1 UMETA(DisplayName = "Median", ToolTip="Median"),
	ModeMin = 2 UMETA(DisplayName = "Mode (Highest)", ToolTip="Mode length (~= highest most common value)"),
	ModeMax = 3 UMETA(DisplayName = "Mode (Lowest)", ToolTip="Mode length (~= lowest most common value)"),
	Central = 4 UMETA(DisplayName = "Central", ToolTip="Central uses the middle value between Min/Max input values."),
	Fixed   = 5 UMETA(DisplayName = "Fixed", ToolTip="Fixed threshold"),
};

namespace PCGExMath
{
	template <typename T>
	FORCEINLINE static T GetAverage(const TArray<T>& Values)
	{
		T Sum = 0;
		for (const T Value : Values) { Sum += Value; }
		return Div(Sum, Values.Num());
	}

	template <typename T>
	FORCEINLINE static T GetMedian(const TArray<T>& Values)
	{
		TArray<T> SortedValues;
		SortedValues.Reserve(Values.Num());
		SortedValues.Append(Values);
		SortedValues.Sort();

		T Median = T{};

		if (SortedValues.IsEmpty()) { Median = 0; }
		else if (SortedValues.Num() == 1) { Median = SortedValues[0]; }
		else
		{
			int32 MiddleIndex = FMath::Floor(SortedValues.Num() / 2);
			if (SortedValues.Num() % 2 == 0) { Median = (SortedValues[MiddleIndex] + SortedValues[MiddleIndex + 1]) / 2; }
			else { Median = SortedValues[MiddleIndex]; }
		}

		return Median;
	}

	PCGEXCORE_API double GetMode(const TArray<double>& Values, const bool bHighest, const uint32 Tolerance = 5);
}
