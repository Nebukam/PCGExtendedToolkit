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
	Relative = 0 UMETA(DisplayName = "Relative", ToolTip="Input value will be normalized between 0..1, or used as a factor. (what it means exactly depends on context. See node-specific documentation.)"),
	Discrete = 1 UMETA(DisplayName = "Discrete", ToolTip="Raw value will be used, or used as absolute. (what it means exactly depends on context. See node-specific documentation.)"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Mean Method"))
enum class EPCGExMeanMethod : uint8
{
	Average = 0 UMETA(DisplayName = "Average", ToolTip="Average"),
	Median  = 1 UMETA(DisplayName = "Median", ToolTip="Median"),
	ModeMin = 2 UMETA(DisplayName = "Mode (Highest)", ToolTip="Mode length (~= highest most common value)"),
	ModeMax = 3 UMETA(DisplayName = "Mode (Lowest)", ToolTip="Mode length (~= lowest most common value)"),
	Central = 4 UMETA(DisplayName = "Central", ToolTip="Central uses the middle value between Min/Max input values."),
	Fixed   = 5 UMETA(DisplayName = "Fixed", ToolTip="Fixed threshold"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Point Bounds Source"))
enum class EPCGExPointBoundsSource : uint8
{
	ScaledBounds  = 0 UMETA(DisplayName = "Scaled Bounds", ToolTip="Scaled Bounds"),
	DensityBounds = 1 UMETA(DisplayName = "Density Bounds", ToolTip="Density Bounds (scaled + steepness)"),
	Bounds        = 2 UMETA(DisplayName = "Bounds", ToolTip="Unscaled Bounds (why?)")
};

namespace PCGExMath
{
	FORCEINLINE static FBox GetLocalBounds(const FPCGPoint& Point, const EPCGExPointBoundsSource Source)
	{
		FVector Extents;

		if (Source == EPCGExPointBoundsSource::ScaledBounds) { Extents = Point.GetScaledExtents(); }
		else if (Source == EPCGExPointBoundsSource::Bounds) { Extents = Point.GetExtents(); }
		else if (Source == EPCGExPointBoundsSource::DensityBounds) { Extents = Point.GetDensityBounds().BoxExtent; }

		return FBox(-Extents, Extents);
	}

#pragma region basics

	FORCEINLINE static double ACot(const double Angle)
	{
		return FMath::Cos(Angle) / FMath::Sin(Angle);
	}

	FORCEINLINE static double DegreesToDot(const double Angle)
	{
		return FMath::Cos(FMath::Clamp(FMath::Abs(Angle), 0, 180.0) * (PI / 180.0));
	}

	FORCEINLINE static double DegreesToDotForComparison(const double Angle)
	{
		return FMath::Cos((180 - FMath::Clamp(FMath::Abs(Angle), 0, 180.0)) * (PI / 180.0));
	}

	FORCEINLINE static double ConvertStringToDouble(const FString& StringToConvert)
	{
		const TCHAR* CharArray = *StringToConvert;
		const double Result = FCString::Atod(CharArray);
		return FMath::IsNaN(Result) ? 0 : Result;
	}

	// Remap function
	FORCEINLINE static double Remap(const double InBase, const double InMin, const double InMax, const double OutMin = 0, const double OutMax = 1)
	{
		return FMath::Lerp(OutMin, OutMax, (InBase - InMin) / (InMax - InMin));
	}

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static T Tile(const T Value, const T Min, const T Max)
	{
		if (FMath::IsWithin(Value, Min, Max)) { return Value; }

		T OutValue = Value;
		T Range = Max - Min + 1;

		OutValue = FMath::Fmod(static_cast<double>(OutValue - Min), static_cast<double>(Range));
		if (OutValue < 0) { OutValue += Range; }

		return OutValue + Min;
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FVector2D Tile(const FVector2D Value, const FVector2D Min, const FVector2D Max)
	{
		return FVector2D(
			Tile(Value.X, Min.X, Max.X),
			Tile(Value.Y, Min.Y, Max.Y));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FVector Tile(const FVector Value, const FVector Min, const FVector Max)
	{
		return FVector(
			Tile(Value.X, Min.X, Max.X),
			Tile(Value.Y, Min.Y, Max.Y),
			Tile(Value.Z, Min.Z, Max.Z));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FVector4 Tile(const FVector4 Value, const FVector4 Min, const FVector4 Max)
	{
		return FVector4(
			Tile(Value.X, Min.X, Max.X),
			Tile(Value.Y, Min.Y, Max.Y),
			Tile(Value.Z, Min.Z, Max.Z),
			Tile(Value.W, Min.W, Max.W));
	}

	template <typename T>
	FORCEINLINE static int SignPlus(const T& InValue)
	{
		const int Sign = FMath::Sign(InValue);
		return Sign == 0 ? 1 : Sign;
	}

	template <typename T>
	FORCEINLINE static int SignMinus(const T& InValue)
	{
		const int Sign = FMath::Sign(InValue);
		return Sign == 0 ? -1 : Sign;
	}

	FORCEINLINE static FBox ConeBox(const FVector& Center, const FVector& ConeDirection, const double Size)
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
	FORCEINLINE static void GetMinMax(const TArray<T>& Values, T& OutMin, T& OutMax)
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
	FORCEINLINE static void SignedNormalize(TArray<T> Values)
	{
		T Min;
		T Max;
		GetMinMax(Values, Min, Max);
		T Range = FMath::Max(FMath::Abs(Max), FMath::Abs(Min));
		for (int i = 0; i < Values.Num(); i++) { Values[i] = Values[i] / Range; }
	}

	template <typename T>
	FORCEINLINE static void Remap(TArray<T> Values, const bool bZeroMin = false, T Range = 1)
	{
		T Min;
		T Max;
		GetMinMax(Values, Min, Max);
		if (bZeroMin) { for (int i = 0; i < Values.Num(); i++) { Values[i] = Remap(Values[i], 0, Max, 0, 1) * Range; } }
		else { for (int i = 0; i < Values.Num(); i++) { Values[i] = Remap(Values[i], Min, Max, 0, 1) * Range; } }
	}

	template <typename T>
	FORCEINLINE static void Remap(TArray<T> Values, T Min, T Max, T Range = 1)
	{
		for (int i = 0; i < Values.Num(); i++) { Values[i] = Remap(Values[i], Min, Max, 0, 1) * Range; }
	}

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

		SortedValues.Empty();
		return Median;
	}

	FORCEINLINE static double GetMode(const TArray<double>& Values, const bool bHighest, const uint32 Tolerance = 5)
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

#pragma region basics

	FORCEINLINE static FVector SafeLinePlaneIntersection(
		const FVector& Pt1, const FVector& Pt2,
		const FVector& PlaneOrigin, const FVector& PlaneNormal,
		bool& bIntersect)
	{
		if (FMath::IsNearlyZero(FVector::DotProduct((Pt1 - Pt2).GetSafeNormal(), PlaneNormal)))
		{
			bIntersect = false;
			return FVector::ZeroVector;
		}

		bIntersect = true;
		return FMath::LinePlaneIntersection(Pt1, Pt2, PlaneOrigin, PlaneNormal);
	}

#pragma endregion

#pragma region Add

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static T Add(const T& A, const T& B) { return A + B; } // Default, unhandled behavior.

	template <typename CompilerSafety = void>
	FORCEINLINE static bool Add(const bool& A, const bool& B) { return B ? true : A; }

	template <typename CompilerSafety = void>
	FORCEINLINE static FRotator Add(const FRotator& A, const FRotator& B) { return A + B; }

	template <typename CompilerSafety = void>
	FORCEINLINE static FQuat Add(const FQuat& A, const FQuat& B) { return Add(A.Rotator(), B.Rotator()).Quaternion(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static FTransform Add(const FTransform& A, const FTransform& B)
	{
		return FTransform(
			A.GetRotation() + B.GetRotation(),
			A.GetLocation() + B.GetLocation(),
			A.GetScale3D() + B.GetScale3D());
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FString Add(const FString& A, const FString& B) { return A < B ? B : A; }

	template <typename CompilerSafety = void>
	FORCEINLINE static FName Add(const FName& A, const FName& B) { return A.ToString() < B.ToString() ? B : A; }

	// Unhandled, but needs to be supported as property
	template <typename CompilerSafety = void>
	FORCEINLINE static FSoftObjectPath Add(const FSoftObjectPath& A, const FSoftObjectPath& B) { return B; }

	template <typename CompilerSafety = void>
	FORCEINLINE static FSoftClassPath Add(const FSoftClassPath& A, const FSoftClassPath& B) { return B; }

#pragma endregion

#pragma region WeightedAdd

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static T WeightedAdd(const T& A, const T& B, const double Weight = 1) { return A + B * Weight; } // Default, unhandled behavior.

	template <typename CompilerSafety = void>
	FORCEINLINE static FRotator WeightedAdd(const FRotator& A, const FRotator& B, const double Weight)
	{
		return FRotator(
			A.Pitch + B.Pitch * Weight,
			A.Yaw + B.Yaw * Weight,
			A.Roll + B.Roll * Weight);
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FQuat WeightedAdd(const FQuat& A, const FQuat& B, const double Weight) { return WeightedAdd(A.Rotator(), B.Rotator(), Weight).Quaternion(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static FTransform WeightedAdd(const FTransform& A, const FTransform& B, const double Weight)
	{
		return FTransform(
			WeightedAdd(A.GetRotation(), B.GetRotation(), Weight),
			WeightedAdd(A.GetLocation(), B.GetLocation(), Weight),
			WeightedAdd(A.GetScale3D(), B.GetScale3D(), Weight));
	}

#define PCGEX_UNSUPPORTED_WEIGHTED_ADD(_TYPE) template <typename CompilerSafety = void> FORCEINLINE static _TYPE WeightedAdd(const _TYPE& A, const _TYPE& B, const double Weight) { return B; }
	PCGEX_UNSUPPORTED_STRING_TYPES(PCGEX_UNSUPPORTED_WEIGHTED_ADD)
	PCGEX_UNSUPPORTED_WEIGHTED_ADD(bool)
#undef PCGEX_UNSUPPORTED_WEIGHTED_ADD

#pragma endregion

#pragma region Sub

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static T Subtract(const T& A, const T& B) { return A - B; }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool Subtract(const bool& A, const bool& B) { return B ? true : A; }

	template <typename CompilerSafety = void>
	FORCEINLINE static FRotator Subtract(const FRotator& A, const FRotator& B) { return B - A; }

	template <typename CompilerSafety = void>
	FORCEINLINE static FQuat Subtract(const FQuat& A, const FQuat& B) { return Subtract(A.Rotator(), B.Rotator()).Quaternion(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static FTransform Subtract(const FTransform& A, const FTransform& B)
	{
		return FTransform(
			Subtract(A.GetRotation(), B.GetRotation()),
			A.GetLocation() - B.GetLocation(),
			A.GetScale3D() - B.GetScale3D());
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FString Subtract(const FString& A, const FString& B) { return A < B ? A : B; }

	template <typename CompilerSafety = void>
	FORCEINLINE static FName Subtract(const FName& A, const FName& B) { return A.ToString() < B.ToString() ? A : B; }

	// Unhandled, but needs to be supported as property
	template <typename CompilerSafety = void>
	FORCEINLINE static FSoftObjectPath Subtract(const FSoftObjectPath& A, const FSoftObjectPath& B) { return A; }

	template <typename CompilerSafety = void>
	FORCEINLINE static FSoftClassPath Subtract(const FSoftClassPath& A, const FSoftClassPath& B) { return A; }

#pragma endregion

#pragma region Min

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static T Min(const T& A, const T& B) { return FMath::Min(A, B); }

	template <typename CompilerSafety = void>
	FORCEINLINE static FVector2D Min(const FVector2D& A, const FVector2D& B)
	{
		return FVector2D(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FVector Min(const FVector& A, const FVector& B)
	{
		return FVector(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y),
			FMath::Min(A.Z, B.Z));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FVector4 Min(const FVector4& A, const FVector4& B)
	{
		return FVector4(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y),
			FMath::Min(A.Z, B.Z),
			FMath::Min(A.W, B.W));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FRotator Min(const FRotator& A, const FRotator& B)
	{
		return FRotator(
			FMath::Min(A.Pitch, B.Pitch),
			FMath::Min(A.Yaw, B.Yaw),
			FMath::Min(A.Roll, B.Roll));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FQuat Min(const FQuat& A, const FQuat& B) { return Min(A.Rotator(), B.Rotator()).Quaternion(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static FTransform Min(const FTransform& A, const FTransform& B)
	{
		return FTransform(
			Min(A.GetRotation(), B.GetRotation()),
			Min(A.GetLocation(), B.GetLocation()),
			Min(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FString Min(const FString& A, const FString& B) { return A > B ? B : A; }

	template <typename CompilerSafety = void>
	FORCEINLINE static FName Min(const FName& A, const FName& B) { return A.ToString() > B.ToString() ? B : A; }

	// Unhandled, but needs to be supported as property
	template <typename CompilerSafety = void>
	FORCEINLINE static FSoftObjectPath Min(const FSoftObjectPath& A, const FSoftObjectPath& B) { return A.ToString() > B.ToString() ? B : A; }

	template <typename CompilerSafety = void>
	FORCEINLINE static FSoftClassPath Min(const FSoftClassPath& A, const FSoftClassPath& B) { return A.ToString() > B.ToString() ? B : A; }

#pragma endregion

#pragma region Max

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static T Max(const T& A, const T& B) { return FMath::Max(A, B); }

	template <typename CompilerSafety = void>
	FORCEINLINE static FVector2D Max(const FVector2D& A, const FVector2D& B)
	{
		return FVector2D(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FVector Max(const FVector& A, const FVector& B)
	{
		return FVector(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y),
			FMath::Max(A.Z, B.Z));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FVector4 Max(const FVector4& A, const FVector4& B)
	{
		return FVector4(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y),
			FMath::Max(A.Z, B.Z),
			FMath::Max(A.W, B.W));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FRotator Max(const FRotator& A, const FRotator& B)
	{
		return FRotator(
			FMath::Max(A.Pitch, B.Pitch),
			FMath::Max(A.Yaw, B.Yaw),
			FMath::Max(A.Roll, B.Roll));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FQuat Max(const FQuat& A, const FQuat& B) { return Max(A.Rotator(), B.Rotator()).Quaternion(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static FTransform Max(const FTransform& A, const FTransform& B)
	{
		return FTransform(
			Max(A.GetRotation(), B.GetRotation()),
			Max(A.GetLocation(), B.GetLocation()),
			Max(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FString Max(const FString& A, const FString& B) { return A > B ? A : B; }

	template <typename CompilerSafety = void>
	FORCEINLINE static FName Max(const FName& A, const FName& B) { return A.ToString() > B.ToString() ? A : B; }

	// Unhandled, but needs to be supported as property
	template <typename CompilerSafety = void>
	FORCEINLINE static FSoftObjectPath Max(const FSoftObjectPath& A, const FSoftObjectPath& B) { return A.ToString() > B.ToString() ? A : B; }

	template <typename CompilerSafety = void>
	FORCEINLINE static FSoftClassPath Max(const FSoftClassPath& A, const FSoftClassPath& B) { return A.ToString() > B.ToString() ? A : B; }

#pragma endregion

#pragma region Lerp

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static T Lerp(const T& A, const T& B, const double& Alpha = 0) { return FMath::Lerp(A, B, Alpha); }

	template <typename CompilerSafety = void>
	FORCEINLINE static FColor Lerp(const FColor& A, const FColor& B, const double& Alpha = 0) { return FMath::Lerp(A.ReinterpretAsLinear(), B.ReinterpretAsLinear(), Alpha).ToFColor(false); }

	template <typename CompilerSafety = void>
	FORCEINLINE static FQuat Lerp(const FQuat& A, const FQuat& B, const double& Alpha = 0) { return FQuat::Slerp(A, B, Alpha); }

	template <typename CompilerSafety = void>
	FORCEINLINE static FTransform Lerp(const FTransform& A, const FTransform& B, const double& Alpha = 0)
	{
		return FTransform(
			Lerp(A.GetRotation(), B.GetRotation(), Alpha),
			Lerp(A.GetLocation(), B.GetLocation(), Alpha),
			Lerp(A.GetScale3D(), B.GetScale3D(), Alpha));
	}

#define PCGEX_UNSUPPORTED_LERP(_TYPE) template <typename CompilerSafety = void> FORCEINLINE static _TYPE Lerp(const _TYPE& A, const _TYPE& B, const double& Alpha = 0) { return Alpha > 0.5 ? B : A; }
	PCGEX_UNSUPPORTED_STRING_TYPES(PCGEX_UNSUPPORTED_LERP)
	PCGEX_UNSUPPORTED_LERP(bool)
#undef PCGEX_UNSUPPORTED_LERP

#pragma endregion

#pragma region Divide

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static T Div(const T& A, const double Divider) { return A / Divider; }

	template <typename CompilerSafety = void>
	FORCEINLINE static FRotator Div(const FRotator& A, const double Divider)
	{
		return FRotator(
			A.Pitch / Divider,
			A.Yaw / Divider,
			A.Roll / Divider);
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FQuat Div(const FQuat& A, const double Divider) { return Div(A.Rotator(), Divider).Quaternion(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static FTransform Div(const FTransform& A, const double Divider)
	{
		return FTransform(
			Div(A.GetRotation(), Divider),
			A.GetLocation() / Divider,
			A.GetScale3D() / Divider);
	}

#define PCGEX_UNSUPPORTED_DIV(_TYPE) template <typename CompilerSafety = void> FORCEINLINE static _TYPE Div(const _TYPE& A, const double Divider) { return A; }
	PCGEX_UNSUPPORTED_STRING_TYPES(PCGEX_UNSUPPORTED_DIV)
	PCGEX_UNSUPPORTED_DIV(bool)
#undef PCGEX_UNSUPPORTED_DIV


#pragma endregion

#pragma region Mult

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static T Mult(const T& A, const T& B) { return A * B; }

	template <typename CompilerSafety = void>
	FORCEINLINE static FRotator Mult(const FRotator& A, const FRotator& B)
	{
		return FRotator(
			A.Pitch * B.Pitch,
			A.Yaw * B.Yaw,
			A.Roll * B.Roll);
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FQuat Mult(const FQuat& A, const FQuat& B) { return Mult(A.Rotator(), B.Rotator()).Quaternion(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static FTransform Mult(const FTransform& A, const FTransform& B)
	{
		return FTransform(
			Mult(A.GetRotation(), B.GetRotation()),
			Mult(A.GetLocation(), B.GetLocation()),
			Mult(A.GetScale3D(), B.GetScale3D()));
	}

#define PCGEX_UNSUPPORTED_MULT(_TYPE) template <typename CompilerSafety = void> FORCEINLINE static _TYPE Mult(const _TYPE& A, const _TYPE& B) { return A; }
	PCGEX_UNSUPPORTED_STRING_TYPES(PCGEX_UNSUPPORTED_MULT)
	PCGEX_UNSUPPORTED_MULT(bool)
#undef PCGEX_UNSUPPORTED_MULT


#pragma endregion

#pragma region Copy

	template <typename T>
	FORCEINLINE static T Copy(const T& A, const T& B, const double& Alpha = 0) { return B; }

#pragma endregion

#pragma region NoBlend

	template <typename T>
	FORCEINLINE static T NoBlend(const T& A, const T& B, const double& Alpha = 0) { return A; }

#pragma endregion

#pragma region Component

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static double GetComponent(const T& A, const int32 Index) { return A; }

	template <typename CompilerSafety = void>
	FORCEINLINE static double GetComponent(const bool& A, const int32 Index) { return A ? 1 : 0; }

	template <typename CompilerSafety = void>
	FORCEINLINE static double GetComponent(const FVector& A, const int32 Index) { return A[Index]; }

	template <typename CompilerSafety = void>
	FORCEINLINE static double GetComponent(const FVector2D& A, const int32 Index) { return A[Index]; }

	template <typename CompilerSafety = void>
	FORCEINLINE static double GetComponent(const FVector4& A, const int32 Index) { return A[Index]; }

	template <typename CompilerSafety = void>
	FORCEINLINE static double GetComponent(const FRotator& A, const int32 Index) { return A.Euler()[Index]; }

	template <typename CompilerSafety = void>
	FORCEINLINE static double GetComponent(const FQuat& A, const int32 Index) { return A.Euler()[Index]; }

	template <typename CompilerSafety = void>
	FORCEINLINE static double GetComponent(const FTransform& A, const int32 Index) { return A.GetLocation()[Index]; }

#define PCGEX_UNSUPPORTED_GET_COMPONENT(_TYPE) template <typename CompilerSafety = void> FORCEINLINE static double GetComponent(const _TYPE& A, const int32 Index) { return -1; }
	PCGEX_UNSUPPORTED_STRING_TYPES(PCGEX_UNSUPPORTED_GET_COMPONENT)
#undef PCGEX_UNSUPPORTED_GET_COMPONENT

	////

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static void SetComponent(T& A, const int32 Index, const double InValue) { A = InValue; }

	template <typename CompilerSafety = void>
	FORCEINLINE static void SetComponent(bool& A, const int32 Index, const double InValue) { A = InValue <= 0 ? false : true; }

	template <typename CompilerSafety = void>
	FORCEINLINE static void SetComponent(FVector& A, const int32 Index, const double InValue) { A[Index] = InValue; }

	template <typename CompilerSafety = void>
	FORCEINLINE static void SetComponent(FVector2D& A, const int32 Index, const double InValue) { A[Index] = InValue; }

	template <typename CompilerSafety = void>
	FORCEINLINE static void SetComponent(FVector4& A, const int32 Index, const double InValue) { A[Index] = InValue; }

	template <typename CompilerSafety = void>
	FORCEINLINE static void SetComponent(FRotator& A, const int32 Index, const double InValue)
	{
		FVector Euler = A.Euler();
		SetComponent(Euler, Index, InValue);
		A = FRotator::MakeFromEuler(Euler);
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static void SetComponent(FQuat& A, const int32 Index, const double InValue)
	{
		FVector Euler = A.Euler();
		SetComponent(Euler, Index, InValue);
		A = FQuat::MakeFromEuler(Euler);
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static void SetComponent(FTransform& A, const int32 Index, const double InValue)
	{
		FVector Location = A.GetLocation();
		SetComponent(Location, Index, InValue);
		A.SetLocation(Location);
	}

#define PCGEX_UNSUPPORTED_SET_COMPONENT(_TYPE) template <typename CompilerSafety = void> FORCEINLINE static void SetComponent(_TYPE& A, const int32 Index, const double InValue)	{}
	PCGEX_UNSUPPORTED_STRING_TYPES(PCGEX_UNSUPPORTED_SET_COMPONENT)
#undef PCGEX_UNSUPPORTED_SET_COMPONENT

#pragma endregion

#pragma region Limits

	template <typename ValueType>
	struct TLimits;

	template <typename ValueType>
	struct TLimits<const ValueType>
		: public TLimits<ValueType>
	{
	};

	template <>
	struct TLimits<bool>
	{
		using ValueType = bool;
		static constexpr ValueType Min() { return false; }
		static constexpr ValueType Max() { return true; }
	};

	template <>
	struct TLimits<int32>
	{
		using ValueType = int32;
		static constexpr ValueType Min() { return TNumericLimits<int32>::Min(); }
		static constexpr ValueType Max() { return TNumericLimits<int32>::Max(); }
	};

	template <>
	struct TLimits<int64>
	{
		using ValueType = int64;
		static constexpr ValueType Min() { return TNumericLimits<int64>::Min(); }
		static constexpr ValueType Max() { return TNumericLimits<int64>::Max(); }
	};

	template <>
	struct TLimits<float>
	{
		using ValueType = float;
		static constexpr ValueType Min() { return TNumericLimits<float>::Min(); }
		static constexpr ValueType Max() { return TNumericLimits<float>::Max(); }
	};

	template <>
	struct TLimits<double>
	{
		using ValueType = double;
		static constexpr ValueType Min() { return TNumericLimits<double>::Min(); }
		static constexpr ValueType Max() { return TNumericLimits<double>::Max(); }
	};

	template <>
	struct TLimits<FVector2D>
	{
		using ValueType = FVector2D;
		static ValueType Min() { return FVector2D(TLimits<double>::Min()); }
		static ValueType Max() { return FVector2D(TLimits<double>::Max()); }
	};

	template <>
	struct TLimits<FVector>
	{
		using ValueType = FVector;
		static ValueType Min() { return FVector(TLimits<double>::Min()); }
		static ValueType Max() { return FVector(TLimits<double>::Max()); }
	};

	template <>
	struct TLimits<FVector4>
	{
		using ValueType = FVector4;
		static ValueType Min() { return FVector4(TLimits<double>::Min()); }
		static ValueType Max() { return FVector4(TLimits<double>::Max()); }
	};

	template <>
	struct TLimits<FRotator>
	{
		using ValueType = FRotator;
		static ValueType Min() { return FRotator(TLimits<double>::Min(), TLimits<double>::Min(), TLimits<double>::Min()); }
		static ValueType Max() { return FRotator(TLimits<double>::Max(), TLimits<double>::Max(), TLimits<double>::Max()); }
	};

	template <>
	struct TLimits<FQuat>
	{
		using ValueType = FQuat;
		static ValueType Min() { return TLimits<FRotator>::Min().Quaternion(); }
		static ValueType Max() { return TLimits<FRotator>::Max().Quaternion(); }
	};

	template <>
	struct TLimits<FTransform>
	{
		using ValueType = FTransform;
		static ValueType Min() { return FTransform::Identity; }
		static ValueType Max() { return FTransform::Identity; }
	};

	template <>
	struct TLimits<FString>
	{
		using ValueType = FString;
		static ValueType Min() { return TEXT(""); }
		static ValueType Max() { return TEXT(""); }
	};

	template <>
	struct TLimits<FName>
	{
		using ValueType = FName;
		static ValueType Min() { return NAME_None; }
		static ValueType Max() { return NAME_None; }
	};

	template <>
	struct TLimits<FSoftClassPath>
	{
		using ValueType = FSoftClassPath;
		static ValueType Min() { return FSoftClassPath(); }
		static ValueType Max() { return FSoftClassPath(); }
	};

	template <>
	struct TLimits<FSoftObjectPath>
	{
		using ValueType = FSoftObjectPath;
		static ValueType Min() { return FSoftObjectPath(); }
		static ValueType Max() { return FSoftObjectPath(); }
	};

#pragma endregion

#pragma region Rounding

	FORCEINLINE static double Round10(const float A) { return FMath::RoundToFloat(A * 10.0f) / 10.0f; }

	FORCEINLINE static FVector Round10(const FVector& A) { return FVector(Round10(A.X), Round10(A.Y), Round10(A.Z)); }


#pragma endregion

#pragma region DoubleMult

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static T DblMult(const T& A, double M) { return A * M; } // Default, unhandled behavior.

	template <typename CompilerSafety = void>
	FORCEINLINE static bool DblMult(const bool& A, double M) { return A; }

	template <typename CompilerSafety = void>
	FORCEINLINE static FRotator DblMult(const FRotator& A, double M) { return A * M; }

	template <typename CompilerSafety = void>
	FORCEINLINE static FQuat DblMult(const FQuat& A, double M) { return (A.Rotator() * M).Quaternion(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static FTransform DblMult(const FTransform& A, double M)
	{
		return FTransform(
			(A.Rotator() * M).Quaternion(),
			A.GetLocation() * M,
			A.GetScale3D() * M);
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FString DblMult(const FString& A, double M) { return A; }

	template <typename CompilerSafety = void>
	FORCEINLINE static FName DblMult(const FName& A, double M) { return A; }

	// Unhandled, but needs to be supported as property
	template <typename CompilerSafety = void>
	FORCEINLINE static FSoftObjectPath DblMult(const FSoftObjectPath& A, const double M) { return A; }

	template <typename CompilerSafety = void>
	FORCEINLINE static FSoftClassPath DblMult(const FSoftClassPath& A, const double M) { return A; }

#pragma endregion


	template <typename T>
	FORCEINLINE static T SanitizeIndex(const T& Index, const T& MaxIndex, const EPCGExIndexSafety Method)
	{
		if (Method == EPCGExIndexSafety::Yoyo)
		{
			const T L = 2 * MaxIndex;
			const T C = Index % L;

			return C <= MaxIndex ? C : L - C;
		}

		switch (Method)
		{
		default:
		case EPCGExIndexSafety::Ignore:
			return (Index < 0 || Index > MaxIndex) ? -1 : Index;
		case EPCGExIndexSafety::Tile:
			return PCGExMath::Tile(Index, 0, MaxIndex);
		case EPCGExIndexSafety::Clamp:
			return FMath::Clamp(Index, 0, MaxIndex);
		}
	}

	FORCEINLINE static FVector GetDirection(const FQuat& Quat, const EPCGExAxis Dir)
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
		}
	}

	FORCEINLINE static FVector GetDirection(const EPCGExAxis Dir)
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
		}
	}

	FORCEINLINE static FQuat MakeDirection(const EPCGExAxis Dir, const FVector& InForward)
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

	FORCEINLINE static FQuat MakeDirection(const EPCGExAxis Dir, const FVector& InForward, const FVector& InUp)
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
	FORCEINLINE static void Swap(TArray<T>& Array, int32 FirstIndex, int32 SecondIndex)
	{
		T* Ptr1 = &Array[FirstIndex];
		T* Ptr2 = &Array[SecondIndex];
		std::swap(*Ptr1, *Ptr2);
	}

	FORCEINLINE static FVector GetNormal(const FVector& A, const FVector& B, const FVector& C)
	{
		return FVector::CrossProduct((B - A), (C - A)).GetSafeNormal();
	}

	FORCEINLINE static FVector GetNormalUp(const FVector& A, const FVector& B, const FVector& Up)
	{
		return FVector::CrossProduct((B - A), ((B + Up) - A)).GetSafeNormal();
	}

	FORCEINLINE static FTransform MakeLookAtTransform(const FVector& LookAt, const FVector& LookUp, const EPCGExAxisAlign AlignAxis)
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

	FORCEINLINE static double GetAngle(const FVector& A, const FVector& B)
	{
		const FVector Cross = FVector::CrossProduct(A, B);
		const double Atan2 = FMath::Atan2(Cross.Size(), A.Dot(B));
		return Cross.Z < 0 ? (PI * 2) - Atan2 : Atan2;
	}

	// Expects normalized vectors
	FORCEINLINE static double GetRadiansBetweenVectors(const FVector& A, const FVector& B, const FVector& UpVector = FVector::UpVector)
	{
		const double Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(A, B), -1.0f, 1.0f));
		return FVector::DotProduct(FVector::CrossProduct(A, B), UpVector) < 0.0f ? -Angle : Angle;
	}

	FORCEINLINE static double GetDegreesBetweenVectors(const FVector& A, const FVector& B, const FVector& UpVector = FVector::UpVector)
	{
		const double D = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(A, B)));
		return FVector::DotProduct(FVector::CrossProduct(A, B), UpVector) < 0 ? 360 - D : D;
	}

	FORCEINLINE static void CheckConvex(const FVector& A, const FVector& B, const FVector& C, bool& bIsConvex, int32& OutSign, const FVector& UpVector = FVector::UpVector)
	{
		if (!bIsConvex) { return; }

		if (A == C)
		{
			bIsConvex = false;
			return;
		}

		const double DP = FVector::DotProduct(FVector::CrossProduct((A - B), (C - A)), UpVector);
		const int32 CurrentSign = (DP > 0.0f) ? 1 : (DP < 0.0f) ? -1 : 0;

		if (CurrentSign != 0)
		{
			if (OutSign == 0) { OutSign = CurrentSign; }
			else if (OutSign != CurrentSign) { bIsConvex = false; }
		}
	};

	FORCEINLINE static FBox ScaledBox(const FBox& InBox, const FVector& InScale)
	{
		const FVector Extents = InBox.GetExtent() * InScale;
		return FBox(-Extents, Extents);
	}

	FORCEINLINE static bool IsDirectionWithinTolerance(const FVector& A, const FVector& B, const FRotator& Limits)
	{
		const FRotator RA = A.Rotation();
		const FRotator RB = B.Rotation();

		return
			FMath::Abs(FRotator::NormalizeAxis(RA.Yaw - RB.Yaw)) <= Limits.Yaw &&
			FMath::Abs(FRotator::NormalizeAxis(RA.Pitch - RB.Pitch)) <= Limits.Pitch &&
			FMath::Abs(FRotator::NormalizeAxis(RA.Roll - RB.Roll)) <= Limits.Roll;
	}

	template <typename T>
	FORCEINLINE static void AtomicMax(T& AtomicValue, T NewValue)
	{
		T CurrentValue = FPlatformAtomics::AtomicRead(&AtomicValue);
		while (NewValue > CurrentValue)
		{
			T PrevValue = FPlatformAtomics::InterlockedCompareExchange(&AtomicValue, NewValue, CurrentValue);
			if (PrevValue == CurrentValue) { break; } // Success: NewValue was stored
			CurrentValue = PrevValue;                 // Retry with updated value
		}
	}

	template <typename T>
	FORCEINLINE static void AtomicMin(T& AtomicValue, T NewValue)
	{
		T CurrentValue = FPlatformAtomics::AtomicRead(&AtomicValue);
		while (NewValue < CurrentValue)
		{
			T PrevValue = FPlatformAtomics::InterlockedCompareExchange(&AtomicValue, NewValue, CurrentValue);
			if (PrevValue == CurrentValue) { break; } // Success: NewValue was stored
			CurrentValue = PrevValue;                 // Retry with updated value
		}
	}

#pragma region Spatialized distances

	// Stolen from PCGDistance
	FORCEINLINE static FVector GetSpatializedCenter(
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
