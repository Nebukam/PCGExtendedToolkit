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

#pragma region MACROS

#define PCGEX_A_B_TPL(_NAME, _BODY) template <typename T, typename CompilerSafety = void> FORCEINLINE static T _NAME(const T& A, const T& B) _BODY
#define PCGEX_A_B(_NAME, _TYPE, _BODY) template <typename CompilerSafety = void> FORCEINLINE static _TYPE _NAME(const _TYPE& A, const _TYPE& B) _BODY
#define PCGEX_A_B2(_NAME, _TYPE) template <typename CompilerSafety = void> FORCEINLINE static _TYPE _NAME(const _TYPE& A, const _TYPE& B) { return _TYPE(_NAME(A[0], B[0]), _NAME(A[1], B[1])); }
#define PCGEX_A_B3(_NAME, _TYPE) template <typename CompilerSafety = void> FORCEINLINE static _TYPE _NAME(const _TYPE& A, const _TYPE& B) { return _TYPE(_NAME(A[0], B[0]), _NAME(A[1], B[1]), _NAME(A[2], B[2])); }
#define PCGEX_A_BR(_NAME) template <typename CompilerSafety = void> FORCEINLINE static FRotator _NAME(const FRotator& A, const FRotator& B) { return FRotator(_NAME(A.Pitch, B.Pitch), _NAME(A.Yaw, B.Yaw), _NAME(A.Roll, B.Roll)); }
#define PCGEX_A_BQ(_NAME) template <typename CompilerSafety = void> FORCEINLINE static FQuat _NAME(const FQuat& A, const FQuat& B) { return _NAME(A.Rotator(), B.Rotator()).Quaternion(); }
#define PCGEX_A_BT(_NAME) template <typename CompilerSafety = void> FORCEINLINE static FTransform _NAME(const FTransform& A, const FTransform& B) { return FTransform(_NAME(A.GetRotation(), B.GetRotation()), _NAME(A.GetLocation(), B.GetLocation()), _NAME(A.GetScale3D(), B.GetScale3D())); }
#define PCGEX_A_B4(_NAME, _TYPE) template <typename CompilerSafety = void> FORCEINLINE static _TYPE _NAME(const _TYPE& A, const _TYPE& B) { return _TYPE(_NAME(A[0], B[0]), _NAME(A[1], B[1]), _NAME(A[2], B[2]), _NAME(A[3], B[3])); }
#define PCGEX_A_B_MULTI(_NAME)\
	PCGEX_A_B2(_NAME, FVector2D)\
	PCGEX_A_B3(_NAME, FVector)\
	PCGEX_A_B4(_NAME, FVector4)\
	PCGEX_A_BR(_NAME)\
	PCGEX_A_BQ(_NAME)\
	PCGEX_A_BT(_NAME)

#define PCGEX_A_B_W_TPL(_NAME, _BODY) template <typename T, typename CompilerSafety = void> FORCEINLINE static T _NAME(const T& A, const T& B, const double& W = 0) _BODY
#define PCGEX_A_B_W(_NAME, _TYPE, _BODY) template <typename CompilerSafety = void> FORCEINLINE static _TYPE _NAME(const _TYPE& A, const _TYPE& B, const double& W) _BODY
#define PCGEX_A_B_W2(_NAME, _TYPE) template <typename CompilerSafety = void> FORCEINLINE static _TYPE _NAME(const _TYPE& A, const _TYPE& B, const double& W) { return _TYPE(_NAME(A[0], B[0], W), _NAME(A[1], B[1], W)); }
#define PCGEX_A_B_W3(_NAME, _TYPE) template <typename CompilerSafety = void> FORCEINLINE static _TYPE _NAME(const _TYPE& A, const _TYPE& B, const double& W) { return _TYPE(_NAME(A[0], B[0], W), _NAME(A[1], B[1], W), _NAME(A[2], B[2], W)); }
#define PCGEX_A_B_WR(_NAME) template <typename CompilerSafety = void> FORCEINLINE static FRotator _NAME(const FRotator& A, const FRotator& B, const double& W) { return FRotator(_NAME(A.Pitch, B.Pitch, W), _NAME(A.Yaw, B.Yaw, W), _NAME(A.Roll, B.Roll, W)); }
#define PCGEX_A_B_WQ(_NAME) template <typename CompilerSafety = void> FORCEINLINE static FQuat _NAME(const FQuat& A, const FQuat& B, const double& W) { return _NAME(A.Rotator(), B.Rotator(), W).Quaternion(); }
#define PCGEX_A_B_WT(_NAME) template <typename CompilerSafety = void> FORCEINLINE static FTransform _NAME(const FTransform& A, const FTransform& B, const double& W) { return FTransform(_NAME(A.GetRotation(), B.GetRotation(), W), _NAME(A.GetLocation(), B.GetLocation(), W), _NAME(A.GetScale3D(), B.GetScale3D(), W)); }
#define PCGEX_A_B_W4(_NAME, _TYPE) template <typename CompilerSafety = void> FORCEINLINE static _TYPE _NAME(const _TYPE& A, const _TYPE& B, const double& W) { return _TYPE(_NAME(A[0], B[0], W), _NAME(A[1], B[1], W), _NAME(A[2], B[2], W), _NAME(A[3], B[3], W)); }
#define PCGEX_A_B_W_MULTI(_NAME)\
	PCGEX_A_B_W2(_NAME, FVector2D)\
	PCGEX_A_B_W3(_NAME, FVector)\
	PCGEX_A_B_W4(_NAME, FVector4)\
	PCGEX_A_B_WR(_NAME)\
	PCGEX_A_B_WQ(_NAME)\
	PCGEX_A_B_WT(_NAME)

#pragma endregion

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
		for (int i = 0; i < Values.Num(); ++i) { Values[i] = Values[i] / Range; }
	}

	template <typename T>
	FORCEINLINE static void Remap(TArray<T> Values, const bool bZeroMin = false, T Range = 1)
	{
		T Min;
		T Max;
		GetMinMax(Values, Min, Max);
		if (bZeroMin) { for (int i = 0; i < Values.Num(); ++i) { Values[i] = Remap(Values[i], 0, Max, 0, 1) * Range; } }
		else { for (int i = 0; i < Values.Num(); ++i) { Values[i] = Remap(Values[i], Min, Max, 0, 1) * Range; } }
	}

	template <typename T>
	FORCEINLINE static void Remap(TArray<T> Values, T Min, T Max, T Range = 1)
	{
		for (int i = 0; i < Values.Num(); ++i) { Values[i] = Remap(Values[i], Min, Max, 0, 1) * Range; }
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

#pragma region Min

	PCGEX_A_B_TPL(Min, { return FMath::Min(A, B); })
	
	PCGEX_A_B_MULTI(Min)
	
	PCGEX_A_B(Min, FString, { return A > B ? B : A; })
	PCGEX_A_B(Min, FName, { return A.ToString() > B.ToString() ? B : A; })
	PCGEX_A_B(Min, FSoftObjectPath, { return A.ToString() > B.ToString() ? B : A; })
	PCGEX_A_B(Min, FSoftClassPath, { return A.ToString() > B.ToString() ? B : A; })

#pragma endregion

#pragma region Max

	PCGEX_A_B_TPL(Max, { return FMath::Max(A, B); })
	
	PCGEX_A_B_MULTI(Max)

	PCGEX_A_B(Max, FString, { return A > B ? A : B; })
	PCGEX_A_B(Max, FName, { return A.ToString() > B.ToString() ? A : B; })
	PCGEX_A_B(Max, FSoftObjectPath, { return A.ToString() > B.ToString() ? A : B; })
	PCGEX_A_B(Max, FSoftClassPath, { return A.ToString() > B.ToString() ? A : B; })

#pragma endregion


#pragma region Add

	PCGEX_A_B_TPL(Add, { return A + B; })
	
	PCGEX_A_B(Add, bool, { return B ? true : A; })
	PCGEX_A_B(Add, FQuat, { return Add(A.Rotator(), B.Rotator()).Quaternion(); })
	PCGEX_A_BT(Add)
	PCGEX_A_B(Add, FString, { return Max(A, B); })
	PCGEX_A_B(Add, FName, { return Max(A, B); })
	PCGEX_A_B(Add, FSoftObjectPath, { return B; })
	PCGEX_A_B(Add, FSoftClassPath, { return B; })

#pragma endregion

#pragma region WeightedAdd

	PCGEX_A_B_W_TPL(WeightedAdd, { return A + B * W; })
	
	PCGEX_A_B_WR(WeightedAdd)
	PCGEX_A_B_W(WeightedAdd, FQuat, { return WeightedAdd(A.Rotator(), B.Rotator(), W).Quaternion(); })
	PCGEX_A_B_WT(WeightedAdd)

	PCGEX_A_B_W(WeightedAdd, bool, { return B; })
	PCGEX_A_B_W(WeightedAdd, FString, { return B; })
	PCGEX_A_B_W(WeightedAdd, FName, { return B; })
	PCGEX_A_B_W(WeightedAdd, FSoftObjectPath, { return B; })
	PCGEX_A_B_W(WeightedAdd, FSoftClassPath, { return B; })

#pragma endregion

#pragma region Sub

	PCGEX_A_B_TPL(Subtract, { return A - B; })
	
	PCGEX_A_B(Subtract, bool, { return B ? true : A; })
	PCGEX_A_B(Subtract, FQuat, { return Subtract(A.Rotator(), B.Rotator()).Quaternion(); })
	PCGEX_A_BT(Subtract)

	PCGEX_A_B(Subtract, FString, { return Min(A, B);})
	PCGEX_A_B(Subtract, FName, { return Min(A, B);})
	PCGEX_A_B(Subtract, FSoftObjectPath, { return Min(A, B);})
	PCGEX_A_B(Subtract, FSoftClassPath, { return Min(A, B);})

#pragma endregion

#pragma region WeightedSub

	PCGEX_A_B_W_TPL(WeightedSub, { return A - B * W; })
	
	PCGEX_A_B_WR(WeightedSub)
	PCGEX_A_B_W(WeightedSub, FQuat, { return WeightedAdd(A.Rotator(), B.Rotator(), W).Quaternion(); })
	PCGEX_A_B_WT(WeightedSub)

	PCGEX_A_B_W(WeightedSub, bool, { return B; })
	PCGEX_A_B_W(WeightedSub, FString, { return B; })
	PCGEX_A_B_W(WeightedSub, FName, { return B; })
	PCGEX_A_B_W(WeightedSub, FSoftObjectPath, { return B; })
	PCGEX_A_B_W(WeightedSub, FSoftClassPath, { return B; })

#pragma endregion

#pragma region UnsignedMin

	PCGEX_A_B_TPL(UnsignedMin, { return FMath::Abs(A) > FMath::Abs(B) ? B : A; })
	
	PCGEX_A_B_MULTI(UnsignedMin)

	PCGEX_A_B(UnsignedMin, bool, { return !A || !B ? false : true; })
	PCGEX_A_B(UnsignedMin, FString, { return Min(A, B); })
	PCGEX_A_B(UnsignedMin, FName, { return Min(A, B); })
	PCGEX_A_B(UnsignedMin, FSoftObjectPath, { return Min(A, B); })
	PCGEX_A_B(UnsignedMin, FSoftClassPath, { return Min(A, B); })

#pragma endregion

#pragma region UnsignedMax

	PCGEX_A_B_TPL(UnsignedMax, { return FMath::Abs(A) > FMath::Abs(B) ? A : B; })
	
	PCGEX_A_B_MULTI(UnsignedMax)

	PCGEX_A_B(UnsignedMax, bool, { return A || B ? true : false; })
	PCGEX_A_B(UnsignedMax, FString, { return Max(A, B); })
	PCGEX_A_B(UnsignedMax, FName, { return Max(A, B); })
	PCGEX_A_B(UnsignedMax, FSoftObjectPath, { return Max(A, B); })
	PCGEX_A_B(UnsignedMax, FSoftClassPath, { return Max(A, B); })

#pragma endregion

#pragma region AbsoluteMin

	PCGEX_A_B_TPL(AbsoluteMin, { return FMath::Min(FMath::Abs(A), FMath::Abs(B)); })
	
	PCGEX_A_B_MULTI(AbsoluteMin)

	PCGEX_A_B(AbsoluteMin, bool, { return !A || !B ? false : true; })
	PCGEX_A_B(AbsoluteMin, FString, { return Min(A, B); })
	PCGEX_A_B(AbsoluteMin, FName, { return Min(A, B); })
	PCGEX_A_B(AbsoluteMin, FSoftObjectPath, { return Min(A, B); })
	PCGEX_A_B(AbsoluteMin, FSoftClassPath, { return Min(A, B); })


#pragma endregion

#pragma region AbsoluteMax

	PCGEX_A_B_TPL(AbsoluteMax, { return FMath::Max(FMath::Abs(A), FMath::Abs(B)); })
	PCGEX_A_B_MULTI(AbsoluteMax)

	PCGEX_A_B(AbsoluteMax, bool, { return A || B ? true : false; })
	PCGEX_A_B(AbsoluteMax, FString, { return Max(A, B); })
	PCGEX_A_B(AbsoluteMax, FName, { return Max(A, B); })
	PCGEX_A_B(AbsoluteMax, FSoftObjectPath, { return Max(A, B); })
	PCGEX_A_B(AbsoluteMax, FSoftClassPath, { return Max(A, B); })

#pragma endregion

#pragma region Lerp

	PCGEX_A_B_W_TPL(Lerp, { return FMath::Lerp(A, B, W); })
	PCGEX_A_B_W(Lerp, FColor, { return FMath::Lerp(A.ReinterpretAsLinear(), B.ReinterpretAsLinear(), W).ToFColor(false);})
	PCGEX_A_B_WR(Lerp)
	PCGEX_A_B_W(Lerp, FQuat, { return FQuat::Slerp(A, B, W); })
	PCGEX_A_B_WT(Lerp)

	PCGEX_A_B_W(Lerp, bool, { return W > 0.5 ? B : A; })
	PCGEX_A_B_W(Lerp, FString, { return W > 0.5 ? B : A; })
	PCGEX_A_B_W(Lerp, FName, { return W > 0.5 ? B : A; })
	PCGEX_A_B_W(Lerp, FSoftObjectPath, { return W > 0.5 ? B : A; })
	PCGEX_A_B_W(Lerp, FSoftClassPath, { return W > 0.5 ? B : A; })

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

	PCGEX_A_B_TPL(Mult, { return A * B; })
	PCGEX_A_B_MULTI(Mult)

	PCGEX_A_B(Mult, bool, { return !A || !B ? false : true; })
	PCGEX_A_B(Mult, FString, { return A; })
	PCGEX_A_B(Mult, FName, { return A; })
	PCGEX_A_B(Mult, FSoftObjectPath, { return A; })
	PCGEX_A_B(Mult, FSoftClassPath, { return A; })

#pragma endregion

#pragma region Copy

	PCGEX_A_B_TPL(Copy, { return B; })

#pragma endregion

#pragma region NoBlend

	PCGEX_A_B_TPL(NoBlend, { return A; })

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

#pragma region Rounding

	FORCEINLINE static double Round10(const float A)
	{
		return FMath::RoundToFloat(A * 10.0f) / 10.0f;
	}

	FORCEINLINE static FVector Round10(const FVector& A)
	{
		return FVector(Round10(A.X), Round10(A.Y), Round10(A.Z));
	}


#pragma endregion

#pragma region DoubleMult

	template
	<
		typename T, typename CompilerSafety = void>
	FORCEINLINE static T DblMult(const T& A, double M)
	{
		return A * M;
	} // Default, unhandled behavior.

	template <typename CompilerSafety = void>
	FORCEINLINE static bool DblMult(const bool& A, double M)
	{
		return A;
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FRotator DblMult(const FRotator& A, double M)
	{
		return A * M;
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static FQuat DblMult(const FQuat& A, double M)
	{
		return (A.Rotator() * M).Quaternion();
	}

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

#undef PCGEX_A_B_TPL
#undef PCGEX_A_B
#undef PCGEX_A_B2
#undef PCGEX_A_B3
#undef PCGEX_A_BR
#undef PCGEX_A_BQ
#undef PCGEX_A_BT
#undef PCGEX_A_B4
#undef PCGEX_A_B_MULTI

#undef PCGEX_A_B_W_TPL
#undef PCGEX_A_B_W
#undef PCGEX_A_B_W2
#undef PCGEX_A_B_W3
#undef PCGEX_A_B_WR
#undef PCGEX_A_B_WQ
#undef PCGEX_A_B_WT
#undef PCGEX_A_B_W4
#undef PCGEX_A_B_W_MULTI
