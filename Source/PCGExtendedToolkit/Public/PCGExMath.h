// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"

#include "PCGExMath.generated.h"

#define PCGEX_UNSUPPORTED_STRING_TYPES(MACRO)\
MACRO(FString)\
MACRO(FName)\
MACRO(FSoftObjectPath)\
MACRO(FSoftClassPath)

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Mean Measure"))
enum class EPCGExMeanMeasure : uint8
{
	Relative UMETA(DisplayName = "Relative", ToolTip="Input value will be normalized between 0..1"),
	Absolute UMETA(DisplayName = "Absolute", ToolTip="Raw value will be used."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Mean Method"))
enum class EPCGExMeanMethod : uint8
{
	Average UMETA(DisplayName = "Average", ToolTip="Average"),
	Median UMETA(DisplayName = "Median", ToolTip="Median"),
	ModeMin UMETA(DisplayName = "Mode (Highest)", ToolTip="Mode length (~= highest most common value)"),
	ModeMax UMETA(DisplayName = "Mode (Lowest)", ToolTip="Mode length (~= lowest most common value)"),
	Central UMETA(DisplayName = "Central", ToolTip="Central uses the middle value between Min/Max input values."),
	Fixed UMETA(DisplayName = "Fixed", ToolTip="Fixed threshold"),
};

namespace PCGExMath
{
#pragma region basics

	/**
	 *	 Leave <---.Apex-----> Arrive (Direction)
	 *		   . '   |    '  .  
	 *		A----Anchor---------B
	 */
	struct PCGEXTENDEDTOOLKIT_API FApex
	{
		FApex()
		{
		}

		FApex(const FVector& Start, const FVector& End, const FVector& InApex)
		{
			Direction = (Start - End).GetSafeNormal();
			Anchor = FMath::ClosestPointOnSegment(InApex, Start, End);

			const double DistToStart = FVector::Dist(Start, Anchor);
			const double DistToEnd = FVector::Dist(End, Anchor);
			TowardStart = Direction * (DistToStart * -1);
			TowardEnd = Direction * DistToEnd;
			Alpha = DistToStart / (DistToStart + DistToEnd);
		}

		FVector Direction;
		FVector Anchor;
		FVector TowardStart;
		FVector TowardEnd;
		double Alpha = 0;

		FVector GetAnchorNormal(const FVector& Location) const { return (Anchor - Location).GetSafeNormal(); }

		void Scale(const double InScale)
		{
			TowardStart *= InScale;
			TowardEnd *= InScale;
		}

		void Extend(const double InSize)
		{
			TowardStart += Direction * InSize;
			TowardEnd += Direction * -InSize;
		}

		static FApex FromStartOnly(const FVector& Start, const FVector& InApex) { return FApex(Start, InApex, InApex); }
		static FApex FromEndOnly(const FVector& End, const FVector& InApex) { return FApex(InApex, End, InApex); }
	};

	struct PCGEXTENDEDTOOLKIT_API FPathMetricsSquared
	{
		FPathMetricsSquared()
		{
		}

		explicit FPathMetricsSquared(const FVector& InStart)
		{
			Add(InStart);
		}

		explicit FPathMetricsSquared(const TArrayView<FPCGPoint>& Points)
		{
			for (const FPCGPoint& Pt : Points) { Add(Pt.Transform.GetLocation()); }
		}

		FPathMetricsSquared(const FPathMetricsSquared& Other)
			: Start(Other.Start),
			  Last(Other.Last),
			  Length(Other.Length),
			  Count(Other.Count)
		{
		}

		FVector Start;
		FVector Last;
		double Length = -1;
		int32 Count = 0;

		void Reset(const FVector& InStart)
		{
			Start = InStart;
			Last = InStart;
			Length = 0;
			Count = 1;
		}

		double Add(const FVector& Location)
		{
			if (Length == -1)
			{
				Reset(Location);
				return 0;
			}
			Length += DistToLast(Location);
			Last = Location;
			Count++;
			return Length;
		}

		bool IsValid() const { return Length > 0; }
		double GetTime(const double Distance) const { return (!Distance || !Length) ? 0 : Distance / Length; }
		double DistToLast(const FVector& Location) const { return FVector::DistSquared(Last, Location); }
		bool IsLastWithinRange(const FVector& Location, const double Range) const { return DistToLast(Location) < Range; }
	};


	struct PCGEXTENDEDTOOLKIT_API FPathMetrics
	{
		FPathMetrics()
		{
		}

		explicit FPathMetrics(const FVector& InStart)
		{
			Add(InStart);
		}

		explicit FPathMetrics(const TArrayView<FPCGPoint>& Points)
		{
			for (const FPCGPoint& Pt : Points) { Add(Pt.Transform.GetLocation()); }
		}

		explicit FPathMetrics(const FPathMetricsSquared& Other)
			: Start(Other.Start),
			  Last(Other.Last),
			  Length(Other.Length),
			  Count(Other.Count)
		{
		}

		FVector Start;
		FVector Last;
		double Length = -1;
		int32 Count = 0;

		void Reset(const FVector& InStart)
		{
			Start = InStart;
			Last = InStart;
			Length = 0;
			Count = 1;
		}

		double Add(const FVector& Location)
		{
			if (Length == -1)
			{
				Reset(Location);
				return 0;
			}
			Length += DistToLast(Location);
			Last = Location;
			Count++;
			return Length;
		}

		bool IsValid() const { return Length > 0; }
		double GetTime(const double Distance) const { return (!Distance || !Length) ? 0 : Distance / Length; }
		double DistToLast(const FVector& Location) const { return FVector::Dist(Last, Location); }
		bool IsLastWithinRange(const FVector& Location, const double Range) const { return DistToLast(Location) < Range; }
	};


	static double DegreesToDot(const double Angle)
	{
		return FMath::Cos(FMath::Clamp(FMath::Abs(Angle), 0, 180.0) * (PI / 180.0));
	}

	static double ConvertStringToDouble(const FString& StringToConvert)
	{
		const TCHAR* CharArray = *StringToConvert;
		const double Result = FCString::Atod(CharArray);
		return FMath::IsNaN(Result) ? 0 : Result;
	}

	// Remap function
	static double Remap(const double InBase, const double InMin, const double InMax, const double OutMin = 0, const double OutMax = 1)
	{
		return FMath::Lerp(OutMin, OutMax, (InBase - InMin) / (InMax - InMin));
	}

	template <typename T, typename CompilerSafety = void>
	static T Tile(const T Value, const T Min, const T Max)
	{
		if (FMath::IsWithin(Value, Min, Max)) { return Value; }

		T OutValue = Value;
		T Range = Max - Min + 1;

		OutValue = FMath::Fmod(static_cast<double>(OutValue - Min), static_cast<double>(Range));
		if (OutValue < 0) { OutValue += Range; }

		return OutValue + Min;
	}

	template <typename CompilerSafety = void>
	static FVector2D Tile(const FVector2D Value, const FVector2D Min, const FVector2D Max)
	{
		return FVector2D(
			Tile(Value.X, Min.X, Max.X),
			Tile(Value.Y, Min.Y, Max.Y));
	}

	template <typename CompilerSafety = void>
	static FVector Tile(const FVector Value, const FVector Min, const FVector Max)
	{
		return FVector(
			Tile(Value.X, Min.X, Max.X),
			Tile(Value.Y, Min.Y, Max.Y),
			Tile(Value.Z, Min.Z, Max.Z));
	}

	template <typename CompilerSafety = void>
	static FVector4 Tile(const FVector4 Value, const FVector4 Min, const FVector4 Max)
	{
		return FVector4(
			Tile(Value.X, Min.X, Max.X),
			Tile(Value.Y, Min.Y, Max.Y),
			Tile(Value.Z, Min.Z, Max.Z),
			Tile(Value.W, Min.W, Max.W));
	}

	template <typename T>
	static int SignPlus(const T& InValue)
	{
		const int Sign = FMath::Sign(InValue);
		return Sign == 0 ? 1 : Sign;
	}

	template <typename T>
	static int SignMinus(const T& InValue)
	{
		const int Sign = FMath::Sign(InValue);
		return Sign == 0 ? -1 : Sign;
	}

	static FBox ConeBox(const FVector& Center, const FVector& ConeDirection, const double Size)
	{
		const FVector Dir = ConeDirection.GetSafeNormal();
		const FVector U = FVector::CrossProduct(Dir, Dir + FVector(0.1)).GetSafeNormal();
		const FVector V = FVector::CrossProduct(Dir, Dir + FVector(-0.1)).GetSafeNormal();

		FBox Box = FBoxCenterAndExtent(Center, FVector(0.0001)).GetBox();
		Box += Center + Dir * Size;
		Box += Center + U * Size;
		Box += Center + V * Size;

		//UE_LOG(LogTemp, Warning, TEXT("Box Min X:%f, Y:%f, Z:%f | Max X:%f, Y:%f, Z:%f"), Box.Min.X, Box.Min.Y, Box.Min.Z, Box.Max.X, Box.Max.Y, Box.Max.Z);

		return Box;
	}

	template <typename T>
	static void GetMinMax(const TArray<T>& Values, T& OutMin, T& OutMax)
	{
		OutMin = TNumericLimits<T>::Max();
		OutMax = TNumericLimits<T>::Min();
		for (const T Value : Values)
		{
			OutMin = FMath::Min(OutMin, Value);
			OutMax = FMath::Max(OutMax, Value);
		}
	}

	template <typename T>
	static void SignedNormalize(TArray<T> Values)
	{
		T Min;
		T Max;
		GetMinMax(Values, Min, Max);
		T Range = FMath::Max(FMath::Abs(Max), FMath::Abs(Min));
		for (int i = 0; i < Values.Num(); i++) { Values[i] = Values[i] / Range; }
	}

	template <typename T>
	static void Remap(TArray<T> Values, const bool bZeroMin = false, T Range = 1)
	{
		T Min;
		T Max;
		GetMinMax(Values, Min, Max);
		if (bZeroMin) { for (int i = 0; i < Values.Num(); i++) { Values[i] = Remap(Values[i], 0, Max, 0, 1) * Range; } }
		else { for (int i = 0; i < Values.Num(); i++) { Values[i] = Remap(Values[i], Min, Max, 0, 1) * Range; } }
	}

	template <typename T>
	static void Remap(TArray<T> Values, T Min, T Max, T Range = 1)
	{
		for (int i = 0; i < Values.Num(); i++) { Values[i] = Remap(Values[i], Min, Max, 0, 1) * Range; }
	}

	template <typename T>
	static T GetAverage(const TArray<T>& Values)
	{
		T Sum = 0;
		for (const T Value : Values) { Sum += Value; }
		return Div(Sum, Values.Num());
	}

	template <typename T>
	static T GetMedian(const TArray<T>& Values)
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

		SortedValues.Empty();
		return Median;
	}

	static double GetMode(const TArray<double>& Values, const bool bHighest, const uint32 Tolerance = 5)
	{
		TMap<double, int32> Map;
		int32 LastCount = 0;
		double Mode = 0;

		for (const double Value : Values)
		{
			const double AdjustedValue = FMath::RoundToZero(Value / Tolerance) * Tolerance;
			const int32* Count = Map.Find(AdjustedValue);
			const int32 UpdatedCount = Count ? *Count + 1 : 1;
			Map.Add(Value, UpdatedCount);

			if (LastCount < UpdatedCount)
			{
				LastCount = UpdatedCount;
				Mode = AdjustedValue;
			}
			else if (LastCount == UpdatedCount)
			{
				if (bHighest) { Mode = FMath::Max(Mode, AdjustedValue); }
				else { Mode = FMath::Min(Mode, AdjustedValue); }
			}
		}

		Map.Empty();
		return Mode;
	}

#pragma endregion

#pragma region Add

	template <typename T, typename CompilerSafety = void>
	static T Add(const T& A, const T& B) { return A + B; } // Default, unhandled behavior.

	template <typename CompilerSafety = void>
	static bool Add(const bool& A, const bool& B) { return B ? true : A; }

	template <typename CompilerSafety = void>
	static FTransform Add(const FTransform& A, const FTransform& B)
	{
		return FTransform(
			A.GetRotation() + B.GetRotation(),
			A.GetLocation() + B.GetLocation(),
			A.GetScale3D() + B.GetScale3D());
	}

	template <typename CompilerSafety = void>
	static FString Add(const FString& A, const FString& B) { return A < B ? B : A; }

	template <typename CompilerSafety = void>
	static FName Add(const FName& A, const FName& B) { return A.ToString() < B.ToString() ? B : A; }

	// Unhandled, but needs to be supported as property
	template <typename CompilerSafety = void>
	static FSoftObjectPath Add(const FSoftObjectPath& A, const FSoftObjectPath& B) { return B; }

	template <typename CompilerSafety = void>
	static FSoftClassPath Add(const FSoftClassPath& A, const FSoftClassPath& B) { return B; }

#pragma endregion

#pragma region Add

	template <typename T, typename CompilerSafety = void>
	static T WeightedAdd(const T& A, const T& B, const double Weight = 1) { return A + B * Weight; } // Default, unhandled behavior.

	template <typename CompilerSafety = void>
	static FTransform WeightedAdd(const FTransform& A, const FTransform& B, const double Weight)
	{
		return FTransform(
			WeightedAdd(A.GetRotation(), B.GetRotation(), Weight),
			WeightedAdd(A.GetLocation(), B.GetLocation(), Weight),
			WeightedAdd(A.GetScale3D(), B.GetScale3D(), Weight));
	}

#define PCGEX_UNSUPPORTED_WEIGHTED_ADD(_TYPE) template <typename CompilerSafety = void> static _TYPE WeightedAdd(const _TYPE& A, const _TYPE& B, const double Weight) { return A; }
	PCGEX_UNSUPPORTED_STRING_TYPES(PCGEX_UNSUPPORTED_WEIGHTED_ADD)
	PCGEX_UNSUPPORTED_WEIGHTED_ADD(bool)
#undef PCGEX_UNSUPPORTED_WEIGHTED_ADD

#pragma endregion


#pragma region Sub

	template <typename T, typename CompilerSafety = void>
	static T Sub(const T& A, const T& B) { return A - B; }

	template <typename CompilerSafety = void>
	static bool Sub(const bool& A, const bool& B) { return B ? true : A; }

	template <typename CompilerSafety = void>
	static FTransform Sub(const FTransform& A, const FTransform& B)
	{
		return FTransform(
			A.GetRotation() - B.GetRotation(),
			A.GetLocation() - B.GetLocation(),
			A.GetScale3D() - B.GetScale3D());
	}

	template <typename CompilerSafety = void>
	static FString Sub(const FString& A, const FString& B) { return A < B ? A : B; }

	template <typename CompilerSafety = void>
	static FName Sub(const FName& A, const FName& B) { return A.ToString() < B.ToString() ? A : B; }

	// Unhandled, but needs to be supported as property
	template <typename CompilerSafety = void>
	static FSoftObjectPath Sub(const FSoftObjectPath& A, const FSoftObjectPath& B) { return A; }

	template <typename CompilerSafety = void>
	static FSoftClassPath Sub(const FSoftClassPath& A, const FSoftClassPath& B) { return A; }

#pragma endregion

#pragma region Min

	template <typename T, typename CompilerSafety = void>
	static T Min(const T& A, const T& B) { return FMath::Min(A, B); }

	template <typename CompilerSafety = void>
	static FVector2D Min(const FVector2D& A, const FVector2D& B)
	{
		return FVector2D(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y));
	}

	template <typename CompilerSafety = void>
	static FVector Min(const FVector& A, const FVector& B)
	{
		return FVector(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y),
			FMath::Min(A.Z, B.Z));
	}

	template <typename CompilerSafety = void>
	static FVector4 Min(const FVector4& A, const FVector4& B)
	{
		return FVector4(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y),
			FMath::Min(A.Z, B.Z),
			FMath::Min(A.W, B.W));
	}

	template <typename CompilerSafety = void>
	static FRotator Min(const FRotator& A, const FRotator& B)
	{
		return FRotator(
			FMath::Min(A.Pitch, B.Pitch),
			FMath::Min(A.Yaw, B.Yaw),
			FMath::Min(A.Roll, B.Roll));
	}

	template <typename CompilerSafety = void>
	static FQuat Min(const FQuat& A, const FQuat& B) { return Min(A.Rotator(), B.Rotator()).Quaternion(); }

	template <typename CompilerSafety = void>
	static FTransform Min(const FTransform& A, const FTransform& B)
	{
		return FTransform(
			Min(A.GetRotation(), B.GetRotation()),
			Min(A.GetLocation(), B.GetLocation()),
			Min(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename CompilerSafety = void>
	static FString Min(const FString& A, const FString& B) { return A > B ? B : A; }

	template <typename CompilerSafety = void>
	static FName Min(const FName& A, const FName& B) { return A.ToString() > B.ToString() ? B : A; }

	// Unhandled, but needs to be supported as property
	template <typename CompilerSafety = void>
	static FSoftObjectPath Min(const FSoftObjectPath& A, const FSoftObjectPath& B) { return A.ToString() > B.ToString() ? B : A; }

	template <typename CompilerSafety = void>
	static FSoftClassPath Min(const FSoftClassPath& A, const FSoftClassPath& B) { return A.ToString() > B.ToString() ? B : A; }

#pragma endregion

#pragma region Max

	template <typename T, typename CompilerSafety = void>
	static T Max(const T& A, const T& B) { return FMath::Max(A, B); }

	template <typename CompilerSafety = void>
	static FVector2D Max(const FVector2D& A, const FVector2D& B)
	{
		return FVector2D(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y));
	}

	template <typename CompilerSafety = void>
	static FVector Max(const FVector& A, const FVector& B)
	{
		return FVector(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y),
			FMath::Max(A.Z, B.Z));
	}

	template <typename CompilerSafety = void>
	static FVector4 Max(const FVector4& A, const FVector4& B)
	{
		return FVector4(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y),
			FMath::Max(A.Z, B.Z),
			FMath::Max(A.W, B.W));
	}

	template <typename CompilerSafety = void>
	static FRotator Max(const FRotator& A, const FRotator& B)
	{
		return FRotator(
			FMath::Max(A.Pitch, B.Pitch),
			FMath::Max(A.Yaw, B.Yaw),
			FMath::Max(A.Roll, B.Roll));
	}

	template <typename CompilerSafety = void>
	static FQuat Max(const FQuat& A, const FQuat& B) { return Max(A.Rotator(), B.Rotator()).Quaternion(); }

	template <typename CompilerSafety = void>
	static FTransform Max(const FTransform& A, const FTransform& B)
	{
		return FTransform(
			Max(A.GetRotation(), B.GetRotation()),
			Max(A.GetLocation(), B.GetLocation()),
			Max(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename CompilerSafety = void>
	static FString Max(const FString& A, const FString& B) { return A > B ? A : B; }

	template <typename CompilerSafety = void>
	static FName Max(const FName& A, const FName& B) { return A.ToString() > B.ToString() ? A : B; }

	// Unhandled, but needs to be supported as property
	template <typename CompilerSafety = void>
	static FSoftObjectPath Max(const FSoftObjectPath& A, const FSoftObjectPath& B) { return A.ToString() > B.ToString() ? A : B; }

	template <typename CompilerSafety = void>
	static FSoftClassPath Max(const FSoftClassPath& A, const FSoftClassPath& B) { return A.ToString() > B.ToString() ? A : B; }

#pragma endregion

#pragma region Lerp

	template <typename T, typename CompilerSafety = void>
	static T Lerp(const T& A, const T& B, const double& Alpha = 0) { return FMath::Lerp(A, B, Alpha); }

	template <typename CompilerSafety = void>
	static FColor Lerp(const FColor& A, const FColor& B, const double& Alpha = 0) { return FMath::Lerp(A.ReinterpretAsLinear(), B.ReinterpretAsLinear(), Alpha).ToFColor(false); }

	template <typename CompilerSafety = void>
	static FQuat Lerp(const FQuat& A, const FQuat& B, const double& Alpha = 0) { return FQuat::Slerp(A, B, Alpha); }

	template <typename CompilerSafety = void>
	static FTransform Lerp(const FTransform& A, const FTransform& B, const double& Alpha = 0)
	{
		return FTransform(
			Lerp(A.GetRotation(), B.GetRotation(), Alpha),
			Lerp(A.GetLocation(), B.GetLocation(), Alpha),
			Lerp(A.GetScale3D(), B.GetScale3D(), Alpha));
	}

#define PCGEX_UNSUPPORTED_LERP(_TYPE) template <typename CompilerSafety = void> static _TYPE Lerp(const _TYPE& A, const _TYPE& B, const double& Alpha = 0) { return Alpha > 0.5 ? B : A; }
	PCGEX_UNSUPPORTED_STRING_TYPES(PCGEX_UNSUPPORTED_LERP)
	PCGEX_UNSUPPORTED_LERP(bool)
#undef PCGEX_UNSUPPORTED_LERP

#pragma endregion

#pragma region Divide

	template <typename T, typename CompilerSafety = void>
	static T Div(const T& A, const double Divider) { return A / Divider; }

	template <typename CompilerSafety = void>
	static FRotator Div(const FRotator& A, const double Divider)
	{
		return FRotator(
			A.Pitch / Divider,
			A.Yaw / Divider,
			A.Roll / Divider);
	}

	template <typename CompilerSafety = void>
	static FQuat Div(const FQuat& A, const double Divider) { return Div(A.Rotator(), Divider).Quaternion(); }

	template <typename CompilerSafety = void>
	static FTransform Div(const FTransform& A, const double Divider)
	{
		return FTransform(
			Div(A.GetRotation(), Divider),
			A.GetLocation() / Divider,
			A.GetScale3D() / Divider);
	}

#define PCGEX_UNSUPPORTED_DIV(_TYPE) template <typename CompilerSafety = void> static _TYPE Div(const _TYPE& A, const double Divider) { return A; }
	PCGEX_UNSUPPORTED_STRING_TYPES(PCGEX_UNSUPPORTED_DIV)
	PCGEX_UNSUPPORTED_DIV(bool)
#undef PCGEX_UNSUPPORTED_DIV


#pragma endregion

#pragma region Mult

	template <typename T, typename CompilerSafety = void>
	static T Mult(const T& A, const T& B) { return A * B; }

	template <typename CompilerSafety = void>
	static FRotator Mult(const FRotator& A, const FRotator& B)
	{
		return FRotator(
			A.Pitch * B.Pitch,
			A.Yaw * B.Yaw,
			A.Roll * B.Roll);
	}

	template <typename CompilerSafety = void>
	static FQuat Mult(const FQuat& A, const FQuat& B) { return Mult(A.Rotator(), B.Rotator()).Quaternion(); }

	template <typename CompilerSafety = void>
	static FTransform Mult(const FTransform& A, const FTransform& B)
	{
		return FTransform(
			Mult(A.GetRotation(), B.GetRotation()),
			Mult(A.GetLocation(), B.GetLocation()),
			Mult(A.GetScale3D(), B.GetScale3D()));
	}

#define PCGEX_UNSUPPORTED_MULT(_TYPE) template <typename CompilerSafety = void> static _TYPE Mult(const _TYPE& A, const _TYPE& B) { return A; }
	PCGEX_UNSUPPORTED_STRING_TYPES(PCGEX_UNSUPPORTED_MULT)
	PCGEX_UNSUPPORTED_MULT(bool)
#undef PCGEX_UNSUPPORTED_MULT


#pragma endregion


#pragma region Copy

	template <typename T>
	static T Copy(const T& A, const T& B, const double& Alpha = 0) { return B; }

#pragma endregion

#pragma region NoBlend

	template <typename T>
	static T NoBlend(const T& A, const T& B, const double& Alpha = 0) { return A; }

#pragma endregion

#pragma region Component

	template <typename T, typename CompilerSafety = void>
	static double GetComponent(const T& A, const int32 Index) { return A; }

	template <typename CompilerSafety = void>
	static double GetComponent(const bool& A, const int32 Index) { return A ? 1 : 0; }

	template <typename CompilerSafety = void>
	static double GetComponent(const FVector& A, const int32 Index) { return A[Index]; }

	template <typename CompilerSafety = void>
	static double GetComponent(const FVector2D& A, const int32 Index) { return A[Index]; }

	template <typename CompilerSafety = void>
	static double GetComponent(const FVector4& A, const int32 Index) { return A[Index]; }

	template <typename CompilerSafety = void>
	static double GetComponent(const FRotator& A, const int32 Index) { return A.Euler()[Index]; }

	template <typename CompilerSafety = void>
	static double GetComponent(const FQuat& A, const int32 Index) { return A.Euler()[Index]; }

	template <typename CompilerSafety = void>
	static double GetComponent(const FTransform& A, const int32 Index) { return A.GetLocation()[Index]; }

#define PCGEX_UNSUPPORTED_GET_COMPONENT(_TYPE) template <typename CompilerSafety = void> static double GetComponent(const _TYPE& A, const int32 Index) { return -1; }
	PCGEX_UNSUPPORTED_STRING_TYPES(PCGEX_UNSUPPORTED_GET_COMPONENT)
#undef PCGEX_UNSUPPORTED_GET_COMPONENT

	////

	template <typename T, typename CompilerSafety = void>
	static void SetComponent(T& A, const int32 Index, const double InValue) { A = InValue; }

	template <typename CompilerSafety = void>
	static void SetComponent(bool& A, const int32 Index, const double InValue) { A = InValue <= 0 ? false : true; }

	template <typename CompilerSafety = void>
	static void SetComponent(FVector& A, const int32 Index, const double InValue) { A[Index] = InValue; }

	template <typename CompilerSafety = void>
	static void SetComponent(FVector2D& A, const int32 Index, const double InValue) { A[Index] = InValue; }

	template <typename CompilerSafety = void>
	static void SetComponent(FVector4& A, const int32 Index, const double InValue) { A[Index] = InValue; }

	template <typename CompilerSafety = void>
	static void SetComponent(FRotator& A, const int32 Index, const double InValue)
	{
		FVector Euler = A.Euler();
		SetComponent(Euler, Index, InValue);
		A = FRotator::MakeFromEuler(Euler);
	}

	template <typename CompilerSafety = void>
	static void SetComponent(FQuat& A, const int32 Index, const double InValue)
	{
		FVector Euler = A.Euler();
		SetComponent(Euler, Index, InValue);
		A = FQuat::MakeFromEuler(Euler);
	}

	template <typename CompilerSafety = void>
	static void SetComponent(FTransform& A, const int32 Index, const double InValue)
	{
		FVector Location = A.GetLocation();
		SetComponent(Location, Index, InValue);
		A.SetLocation(Location);
	}

#define PCGEX_UNSUPPORTED_SET_COMPONENT(_TYPE) template <typename CompilerSafety = void> static void SetComponent(_TYPE& A, const int32 Index, const double InValue)	{}
	PCGEX_UNSUPPORTED_STRING_TYPES(PCGEX_UNSUPPORTED_SET_COMPONENT)
#undef PCGEX_UNSUPPORTED_SET_COMPONENT

#pragma endregion

	template <typename T>
	static T SanitizeIndex(const T& Index, const T& Limit, const EPCGExIndexSafety Method)
	{
		switch (Method)
		{
		case EPCGExIndexSafety::Ignore:
			if (Index < 0 || Index > Limit) { return -1; }
			break;
		case EPCGExIndexSafety::Tile:
			return PCGExMath::Tile(Index, 0, Limit);
		case EPCGExIndexSafety::Clamp:
			return FMath::Clamp(Index, 0, Limit);
		default: ;
		}
		return Index;
	}

	static FVector GetDirection(const FQuat& Quat, const EPCGExAxis Dir)
	{
		switch (Dir)
		{
		default:
		case EPCGExAxis::Forward:
			return Quat.GetForwardVector();
		case EPCGExAxis::Backward:
			return Quat.GetForwardVector() * -1;
		case EPCGExAxis::Right:
			return Quat.GetRightVector();
		case EPCGExAxis::Left:
			return Quat.GetRightVector() * -1;
		case EPCGExAxis::Up:
			return Quat.GetUpVector();
		case EPCGExAxis::Down:
			return Quat.GetUpVector() * -1;
		case EPCGExAxis::Euler:
			return Quat.Euler() * -1;
		}
	}

	static FVector GetDirection(const EPCGExAxis Dir)
	{
		switch (Dir)
		{
		default:
		case EPCGExAxis::Forward:
			return FVector::ForwardVector;
		case EPCGExAxis::Backward:
			return FVector::BackwardVector;
		case EPCGExAxis::Right:
			return FVector::RightVector;
		case EPCGExAxis::Left:
			return FVector::LeftVector;
		case EPCGExAxis::Up:
			return FVector::UpVector;
		case EPCGExAxis::Down:
			return FVector::DownVector;
		case EPCGExAxis::Euler:
			return FVector::OneVector;
		}
	}

	static FQuat MakeDirection(const EPCGExAxis Dir, const FVector& InForward)
	{
		switch (Dir)
		{
		default:
		case EPCGExAxis::Forward:
			return FRotationMatrix::MakeFromX(InForward * -1).ToQuat();
		case EPCGExAxis::Backward:
			return FRotationMatrix::MakeFromX(InForward).ToQuat();
		case EPCGExAxis::Right:
			return FRotationMatrix::MakeFromY(InForward * -1).ToQuat();
		case EPCGExAxis::Left:
			return FRotationMatrix::MakeFromY(InForward).ToQuat();
		case EPCGExAxis::Up:
			return FRotationMatrix::MakeFromZ(InForward * -1).ToQuat();
		case EPCGExAxis::Down:
			return FRotationMatrix::MakeFromZ(InForward).ToQuat();
		}
	}

	static FQuat MakeDirection(const EPCGExAxis Dir, const FVector& InForward, const FVector& InUp)
	{
		switch (Dir)
		{
		default:
		case EPCGExAxis::Forward:
			return FRotationMatrix::MakeFromXZ(InForward * -1, InUp).ToQuat();
		case EPCGExAxis::Backward:
			return FRotationMatrix::MakeFromXZ(InForward, InUp).ToQuat();
		case EPCGExAxis::Right:
			return FRotationMatrix::MakeFromYZ(InForward * -1, InUp).ToQuat();
		case EPCGExAxis::Left:
			return FRotationMatrix::MakeFromYZ(InForward, InUp).ToQuat();
		case EPCGExAxis::Up:
			return FRotationMatrix::MakeFromZY(InForward * -1, InUp).ToQuat();
		case EPCGExAxis::Down:
			return FRotationMatrix::MakeFromZY(InForward, InUp).ToQuat();
		}
	}

	template <typename T>
	static void Swap(TArray<T>& Array, int32 FirstIndex, int32 SecondIndex)
	{
		T* Ptr1 = &Array[FirstIndex];
		T* Ptr2 = &Array[SecondIndex];
		std::swap(*Ptr1, *Ptr2);
	}

	static void RandomizeSeed(FPCGPoint& Point, const FVector& Offset = FVector::ZeroVector)
	{
		Point.Seed = static_cast<int32>(Remap(
			FMath::PerlinNoise3D(Tile(Point.Transform.GetLocation() * 0.001 + Offset, FVector(-1), FVector(1))),
			-1, 1, TNumericLimits<int32>::Min(), TNumericLimits<int32>::Max()));
	}

	static FVector GetNormal(const FVector& A, const FVector& B, const FVector& C)
	{
		return FVector::CrossProduct((B - A), (C - A)).GetSafeNormal();
	}

	static FTransform MakeLookAtTransform(const FVector& LookAt, const FVector& LookUp, const EPCGExAxisAlign AlignAxis)
	{
		switch (AlignAxis)
		{
		case EPCGExAxisAlign::Forward:
			return FTransform(FRotationMatrix::MakeFromXZ(LookAt * -1, LookUp));
		case EPCGExAxisAlign::Backward:
			return FTransform(FRotationMatrix::MakeFromXZ(LookAt, LookUp));
		case EPCGExAxisAlign::Right:
			return FTransform(FRotationMatrix::MakeFromYZ(LookAt * -1, LookUp));
		case EPCGExAxisAlign::Left:
			return FTransform(FRotationMatrix::MakeFromYZ(LookAt, LookUp));
		case EPCGExAxisAlign::Up:
			return FTransform(FRotationMatrix::MakeFromZY(LookAt * -1, LookUp));
		case EPCGExAxisAlign::Down:
			return FTransform(FRotationMatrix::MakeFromZY(LookAt, LookUp));
		default: return FTransform::Identity;
		}
	}

	static double GetAngle(const FVector& A, const FVector& B)
	{
		const FVector Cross = FVector::CrossProduct(A, B);
		const double Atan2 = FMath::Atan2(Cross.Size(), A.Dot(B));
		return Cross.Z < 0 ? (PI * 2) - Atan2 : Atan2;
	}

#pragma region Spatialized distances

	// Stolen from PCGDistance
	static FVector GetSpatializedCenter(
		const EPCGExDistance Shape,
		const FPCGPoint& FromPoint,
		const FVector& FromCenter,
		const FVector& ToCenter)
	{
		if (Shape == EPCGExDistance::SphereBounds)
		{
			FVector Dir = ToCenter - FromCenter;
			Dir.Normalize();

			return FromCenter + Dir * FromPoint.GetScaledExtents().Length();
		}
		if (Shape == EPCGExDistance::BoxBounds)
		{
			const FVector LocalTargetCenter = FromPoint.Transform.InverseTransformPosition(ToCenter);

			const double DistanceSquared = ComputeSquaredDistanceFromBoxToPoint(FromPoint.BoundsMin, FromPoint.BoundsMax, LocalTargetCenter);

			FVector Dir = -LocalTargetCenter;
			Dir.Normalize();

			const FVector LocalClosestPoint = LocalTargetCenter + Dir * FMath::Sqrt(DistanceSquared);

			return FromPoint.Transform.TransformPosition(LocalClosestPoint);
		}

		// EPCGExDistance::Center
		return FromCenter;
	}

#pragma endregion
}

#undef PCGEX_UNSUPPORTED_STRING_TYPES
