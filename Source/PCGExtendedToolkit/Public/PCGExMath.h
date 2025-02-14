// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExH.h"

#include "PCGExMath.generated.h"

#define MIN_dbl_neg MAX_dbl *-1

UENUM()
enum class EPCGExMeanMeasure : uint8
{
	Relative = 0 UMETA(DisplayName = "Relative", ToolTip="Input value will be normalized between 0..1, or used as a factor. (what it means exactly depends on context. See node-specific documentation.)"),
	Discrete = 1 UMETA(DisplayName = "Discrete", ToolTip="Raw value will be used, or used as absolute. (what it means exactly depends on context. See node-specific documentation.)"),
};

UENUM()
enum class EPCGExMeanMethod : uint8
{
	Average = 0 UMETA(DisplayName = "Average", ToolTip="Average"),
	Median  = 1 UMETA(DisplayName = "Median", ToolTip="Median"),
	ModeMin = 2 UMETA(DisplayName = "Mode (Highest)", ToolTip="Mode length (~= highest most common value)"),
	ModeMax = 3 UMETA(DisplayName = "Mode (Lowest)", ToolTip="Mode length (~= lowest most common value)"),
	Central = 4 UMETA(DisplayName = "Central", ToolTip="Central uses the middle value between Min/Max input values."),
	Fixed   = 5 UMETA(DisplayName = "Fixed", ToolTip="Fixed threshold"),
};

UENUM()
enum class EPCGExPointBoundsSource : uint8
{
	ScaledBounds  = 0 UMETA(DisplayName = "Scaled Bounds", ToolTip="Scaled Bounds"),
	DensityBounds = 1 UMETA(DisplayName = "Density Bounds", ToolTip="Density Bounds (scaled + steepness)"),
	Bounds        = 2 UMETA(DisplayName = "Bounds", ToolTip="Unscaled Bounds (why?)")
};

namespace PCGExMath
{
	template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds>
	static FBox GetLocalBounds(const FPCGPoint& Point)
	{
		if constexpr (S == EPCGExPointBoundsSource::ScaledBounds)
		{
			const FVector Scale = Point.Transform.GetScale3D();
			return FBox(Point.BoundsMin * Scale, Point.BoundsMax * Scale);
		}
		else if constexpr (S == EPCGExPointBoundsSource::Bounds)
		{
			return Point.GetLocalBounds();
		}
		else if constexpr (S == EPCGExPointBoundsSource::DensityBounds)
		{
			return Point.GetLocalDensityBounds();
		}
		else
		{
			return FBox(FVector::OneVector * -1, FVector::OneVector);
		}
	}

	template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds>
	static FBox GetLocalBounds(const FPCGPoint* Point)
	{
		if constexpr (S == EPCGExPointBoundsSource::ScaledBounds)
		{
			const FBox LocalBounds = Point->GetLocalBounds();
			const FVector Scale = Point->Transform.GetScale3D();
			return FBox(LocalBounds.Min * Scale, LocalBounds.Max * Scale);
		}
		else if constexpr (S == EPCGExPointBoundsSource::Bounds)
		{
			return Point->GetLocalBounds();
		}
		else if constexpr (S == EPCGExPointBoundsSource::DensityBounds)
		{
			return Point->GetLocalDensityBounds();
		}
		else
		{
			return FBox(FVector::OneVector * -1, FVector::OneVector);
		}
	}

	FBox GetLocalBounds(const FPCGPoint& Point, const EPCGExPointBoundsSource Source);
	FBox GetLocalBounds(const FPCGPoint* Point, const EPCGExPointBoundsSource Source);

#pragma region basics

	FORCEINLINE static double DegreesToDot(const double Angle)
	{
		return FMath::Cos(FMath::Clamp(FMath::Abs(Angle), 0, 180.0) * (PI / 180.0));
	}

	FORCEINLINE static double DegreesToDotForComparison(const double Angle)
	{
		return FMath::Cos((180 - FMath::Clamp(FMath::Abs(Angle), 0, 180.0)) * (PI / 180.0));
	}

	double ConvertStringToDouble(const FString& StringToConvert);

	// Remap function
	FORCEINLINE static double Remap(const double InBase, const double InMin, const double InMax, const double OutMin = 0, const double OutMax = 1)
	{
		return FMath::Lerp(OutMin, OutMax, (InBase - InMin) / (InMax - InMin));
	}

	template <typename T>
	FORCEINLINE static T Tile(const T Value, const T Min, const T Max)
	{
		if constexpr (std::is_unsigned_v<T>)
		{
			return ((Value - Min) % (Max - Min + 1)) + Min;
		}
		else if constexpr (std::is_integral_v<T>)
		{
			T R = Max - Min + 1;
			T W = (Value - Min) % R;
			return W < 0 ? W + R + Min : W + Min;
		}
		else if constexpr (std::is_floating_point_v<T>)
		{
			T R = Max - Min;
			T W = FMath::Fmod(Value - Min, R);
			return W < 0 ? W + R + Min : W + Min;
		}
		else if constexpr (std::is_same_v<T, FVector2D>)
		{
			return FVector2D(
				Tile(Value.X, Min.X, Max.X),
				Tile(Value.Y, Min.Y, Max.Y));
		}
		else if constexpr (std::is_same_v<T, FVector>)
		{
			return FVector(
				Tile(Value.X, Min.X, Max.X),
				Tile(Value.Y, Min.Y, Max.Y),
				Tile(Value.Z, Min.Z, Max.Z));
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			return FVector4(
				Tile(Value.X, Min.X, Max.X),
				Tile(Value.Y, Min.Y, Max.Y),
				Tile(Value.Z, Min.Z, Max.Z),
				Tile(Value.W, Min.W, Max.W));
		}
		else
		{
			static_assert(
				std::is_unsigned_v<T> ||
				std::is_integral_v<T> ||
				std::is_floating_point_v<T> ||
				std::is_same_v<T, FVector2D> ||
				std::is_same_v<T, FVector> ||
				std::is_same_v<T, FVector4>, "Can't tile type.");
			return T{};
		}
	}

	template <typename T>
	FORCEINLINE static T Abs(const T& A)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return A;
		}
		else if constexpr (std::is_same_v<T, FVector2D>)
		{
			return FVector2D(FMath::Abs(A.X), FMath::Abs(A.Y));
		}
		else if constexpr (std::is_same_v<T, FVector>)
		{
			return FVector(FMath::Abs(A.X), FMath::Abs(A.Y), FMath::Abs(A.Z));
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			return FVector4(FMath::Abs(A.X), FMath::Abs(A.Y), FMath::Abs(A.Z), FMath::Abs(A.W));
		}
		else if constexpr (std::is_same_v<T, FQuat>)
		{
			return Abs(A.Rotator()).Quaternion().GetNormalized();
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			return FRotator(FMath::Abs(A.Pitch), FMath::Abs(A.Yaw), FMath::Abs(A.Roll));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return FTransform(Abs(A.GetRotation()), Abs(A), Abs(A.GetScale3D()));
		}
		else if constexpr (std::is_same_v<T, FString> || std::is_same_v<T, FName> || std::is_same_v<T, FSoftClassPath> || std::is_same_v<T, FSoftObjectPath>)
		{
			return Max(A, A);
		}
		else
		{
			return FMath::Abs(A);
		}
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

		FBox Box = FBox(Center - FVector(0.0001), Center + FVector(0.0001));
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

		return Median;
	}

	double GetMode(const TArray<double>& Values, const bool bHighest, const uint32 Tolerance = 5);

	FVector SafeLinePlaneIntersection(
		const FVector& Pt1, const FVector& Pt2,
		const FVector& PlaneOrigin, const FVector& PlaneNormal,
		bool& bIntersect);


	bool SphereOverlap(const FSphere& S1, const FSphere& S2, double& OutOverlap);
	bool SphereOverlap(const FBoxSphereBounds& S1, const FBoxSphereBounds& S2, double& OutOverlap);


#pragma endregion

#pragma region Ops

	template <typename T>
	FORCEINLINE static T Min(const T& A, const T& B)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return A || B;
		}
		else if constexpr (std::is_same_v<T, FVector2D>)
		{
			return FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
		}
		else if constexpr (std::is_same_v<T, FVector>)
		{
			return FVector(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y), FMath::Min(A.Z, B.Z));
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			return FVector4(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y), FMath::Min(A.Z, B.Z), FMath::Min(A.W, B.W));
		}
		else if constexpr (std::is_same_v<T, FColor>)
		{
			return FColor(FMath::Min(A.R, B.R), FMath::Min(A.G, B.G), FMath::Min(A.B, B.B), FMath::Min(A.A, B.A));
		}
		else if constexpr (std::is_same_v<T, FQuat>)
		{
			return Min(A.Rotator(), B.Rotator()).Quaternion();
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			return FRotator(FMath::Min(A.Pitch, B.Pitch), FMath::Min(A.Yaw, B.Yaw), FMath::Min(A.Roll, B.Roll));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return FTransform(Min(A.GetRotation(), B.GetRotation()), Min(A.GetLocation(), B.GetLocation()), Min(A.GetScale3D(), B.GetScale3D()));
		}
		else if constexpr (std::is_same_v<T, FString>)
		{
			return A > B ? B : A;
		}
		else if constexpr (
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftClassPath> ||
			std::is_same_v<T, FSoftObjectPath>)
		{
			return A.ToString() > B.ToString() ? B : A;
		}
		else
		{
			return FMath::Min(A, B);
		}
	}

	template <typename T>
	FORCEINLINE static T Max(const T& A, const T& B)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return A || B;
		}
		else if constexpr (std::is_same_v<T, FVector2D>)
		{
			return FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y));
		}
		else if constexpr (std::is_same_v<T, FVector>)
		{
			return FVector(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y), FMath::Max(A.Z, B.Z));
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			return FVector4(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y), FMath::Max(A.Z, B.Z), FMath::Max(A.W, B.W));
		}
		else if constexpr (std::is_same_v<T, FColor>)
		{
			return FColor(FMath::Max(A.R, B.R), FMath::Max(A.G, B.G), FMath::Max(A.B, B.B), FMath::Max(A.A, B.A));
		}
		else if constexpr (std::is_same_v<T, FQuat>)
		{
			return Max(A.Rotator(), B.Rotator()).Quaternion();
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			return FRotator(FMath::Max(A.Pitch, B.Pitch), FMath::Max(A.Yaw, B.Yaw), FMath::Max(A.Roll, B.Roll));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return FTransform(Max(A.GetRotation(), B.GetRotation()), Max(A.GetLocation(), B.GetLocation()), Max(A.GetScale3D(), B.GetScale3D()));
		}
		else if constexpr (std::is_same_v<T, FString>)
		{
			return A < B ? B : A;
		}
		else if constexpr (
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftClassPath> ||
			std::is_same_v<T, FSoftObjectPath>)
		{
			return A.ToString() < B.ToString() ? B : A;
		}
		else
		{
			return FMath::Max(A, B);
		}
	}

	template <typename T>
	FORCEINLINE static T Add(const T& A, const T& B)
	{
		if constexpr (std::is_same_v<T, FQuat>)
		{
			return Add(A.Rotator(), B.Rotator()).Quaternion();
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return FTransform(Add(A.GetRotation(), B.GetRotation()), Add(A.GetLocation(), B.GetLocation()), Add(A.GetScale3D(), B.GetScale3D()));
		}
		else if constexpr (std::is_same_v<T, FString>)
		{
			return A + B;
		}
		else if constexpr (std::is_same_v<T, FName>)
		{
			return FName(A.ToString() + B.ToString());
		}
		else if constexpr (
			std::is_same_v<T, bool> ||
			std::is_same_v<T, FSoftObjectPath> ||
			std::is_same_v<T, FSoftClassPath>)
		{
			return Max(A, B);
		}
		else
		{
			return A + B;
		}
	}

	template <typename T>
	FORCEINLINE static T WeightedAdd(const T& A, const T& B, const double& W = 0)
	{
		if constexpr (std::is_same_v<T, FQuat>)
		{
			return WeightedAdd(A.Rotator(), B.Rotator(), W).Quaternion();
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			return FRotator(WeightedAdd(A.Pitch, B.Pitch, W), WeightedAdd(A.Yaw, B.Yaw, W), WeightedAdd(A.Roll, B.Roll, W));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return FTransform(WeightedAdd(A.GetRotation(), B.GetRotation(), W), WeightedAdd(A.GetLocation(), B.GetLocation(), W), WeightedAdd(A.GetScale3D(), B.GetScale3D(), W));
		}
		else if constexpr (
			std::is_same_v<T, FString> ||
			std::is_same_v<T, FName>)
		{
			return Add(A, B);
		}
		else if constexpr (
			std::is_same_v<T, bool> ||
			std::is_same_v<T, FSoftObjectPath> ||
			std::is_same_v<T, FSoftClassPath>)
		{
			return Max(A, B);
		}
		else
		{
			return A + B * W;
		}
	}

	template <typename T>
	FORCEINLINE static T Sub(const T& A, const T& B, const double& W = 0)
	{
		if constexpr (std::is_same_v<T, FQuat>)
		{
			return Sub(A.Rotator(), B.Rotator(), W).Quaternion();
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			return FRotator(Sub(A.Pitch, B.Pitch, W), Sub(A.Yaw, B.Yaw, W), Sub(A.Roll, B.Roll, W));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return FTransform(Sub(A.GetRotation(), B.GetRotation(), W), Sub(A.GetLocation(), B.GetLocation(), W), Sub(A.GetScale3D(), B.GetScale3D(), W));
		}
		else if constexpr (
			std::is_same_v<T, bool> ||
			std::is_same_v<T, FString> ||
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftObjectPath> ||
			std::is_same_v<T, FSoftClassPath>)
		{
			return Min(A, B);
		}
		else
		{
			return A - B;
		}
	}

	template <typename T>
	FORCEINLINE static T WeightedSub(const T& A, const T& B, const double& W = 0)
	{
		if constexpr (std::is_same_v<T, FQuat>)
		{
			return WeightedSub(A.Rotator(), B.Rotator(), W).Quaternion();
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			return FRotator(WeightedSub(A.Pitch, B.Pitch, W), WeightedSub(A.Yaw, B.Yaw, W), WeightedSub(A.Roll, B.Roll, W));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return FTransform(WeightedSub(A.GetRotation(), B.GetRotation(), W).GetNormalized(), WeightedSub(A.GetLocation(), B.GetLocation(), W), WeightedSub(A.GetScale3D(), B.GetScale3D(), W));
		}
		else if constexpr (
			std::is_same_v<T, bool> ||
			std::is_same_v<T, FString> ||
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftObjectPath> ||
			std::is_same_v<T, FSoftClassPath>)
		{
			return Min(A, B);
		}
		else
		{
			return A - B * W;
		}
	}

	template <typename T>
	FORCEINLINE static T UnsignedMin(const T& A, const T& B)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return !A || !B ? false : true;
		}
		else if constexpr (std::is_same_v<T, FVector2D>)
		{
			return FVector2D(UnsignedMin(A.X, B.X), UnsignedMin(A.Y, B.Y));
		}
		else if constexpr (std::is_same_v<T, FVector>)
		{
			return FVector(UnsignedMin(A.X, B.X), UnsignedMin(A.Y, B.Y), UnsignedMin(A.Z, B.Z));
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			return FVector4(UnsignedMin(A.X, B.X), UnsignedMin(A.Y, B.Y), UnsignedMin(A.Z, B.Z), UnsignedMin(A.W, B.W));
		}
		else if constexpr (std::is_same_v<T, FQuat>)
		{
			return Min(A.Rotator(), B.Rotator()).Quaternion();
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			return FRotator(UnsignedMin(A.Pitch, B.Pitch), UnsignedMin(A.Yaw, B.Yaw), UnsignedMin(A.Roll, B.Roll));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return FTransform(UnsignedMin(A.GetRotation(), B.GetRotation()), UnsignedMin(A.GetLocation(), B.GetLocation()), UnsignedMin(A.GetScale3D(), B.GetScale3D()));
		}
		else if constexpr (
			std::is_same_v<T, FString> ||
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftClassPath> ||
			std::is_same_v<T, FSoftObjectPath>)
		{
			return Min(B, A);
		}
		else
		{
			return FMath::Abs(A) > FMath::Abs(B) ? B : A;
		}
	}

	template <typename T>
	FORCEINLINE static T UnsignedMax(const T& A, const T& B)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return A || B ? true : false;
		}
		else if constexpr (std::is_same_v<T, FVector2D>)
		{
			return FVector2D(UnsignedMax(A.X, B.X), UnsignedMax(A.Y, B.Y));
		}
		else if constexpr (std::is_same_v<T, FVector>)
		{
			return FVector(UnsignedMax(A.X, B.X), UnsignedMax(A.Y, B.Y), UnsignedMax(A.Z, B.Z));
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			return FVector4(UnsignedMax(A.X, B.X), UnsignedMax(A.Y, B.Y), UnsignedMax(A.Z, B.Z), UnsignedMax(A.W, B.W));
		}
		else if constexpr (std::is_same_v<T, FQuat>)
		{
			return Min(A.Rotator(), B.Rotator()).Quaternion();
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			return FRotator(UnsignedMax(A.Pitch, B.Pitch), UnsignedMax(A.Yaw, B.Yaw), UnsignedMax(A.Roll, B.Roll));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return FTransform(UnsignedMax(A.GetRotation(), B.GetRotation()), UnsignedMax(A.GetLocation(), B.GetLocation()), UnsignedMax(A.GetScale3D(), B.GetScale3D()));
		}
		else if constexpr (
			std::is_same_v<T, FString> ||
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftClassPath> ||
			std::is_same_v<T, FSoftObjectPath>)
		{
			return Max(B, A);
		}
		else
		{
			return FMath::Abs(A) < FMath::Abs(B) ? B : A;
		}
	}

	template <typename T>
	FORCEINLINE static T AbsoluteMin(const T& A, const T& B)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return A || B ? true : false;
		}
		else if constexpr (std::is_same_v<T, FVector2D>)
		{
			return FVector2D(AbsoluteMin(A.X, B.X), AbsoluteMin(A.Y, B.Y));
		}
		else if constexpr (std::is_same_v<T, FVector>)
		{
			return FVector(AbsoluteMin(A.X, B.X), AbsoluteMin(A.Y, B.Y), AbsoluteMin(A.Z, B.Z));
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			return FVector4(AbsoluteMin(A.X, B.X), AbsoluteMin(A.Y, B.Y), AbsoluteMin(A.Z, B.Z), AbsoluteMin(A.W, B.W));
		}
		else if constexpr (std::is_same_v<T, FQuat>)
		{
			return Min(A.Rotator(), B.Rotator()).Quaternion();
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			return FRotator(AbsoluteMin(A.Pitch, B.Pitch), AbsoluteMin(A.Yaw, B.Yaw), AbsoluteMin(A.Roll, B.Roll));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return FTransform(AbsoluteMin(A.GetRotation(), B.GetRotation()), AbsoluteMin(A.GetLocation(), B.GetLocation()), AbsoluteMin(A.GetScale3D(), B.GetScale3D()));
		}
		else if constexpr (
			std::is_same_v<T, FString> ||
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftClassPath> ||
			std::is_same_v<T, FSoftObjectPath>)
		{
			return Min(B, A);
		}
		else
		{
			return FMath::Min(FMath::Abs(A), FMath::Abs(B));
		}
	}

	template <typename T>
	FORCEINLINE static T AbsoluteMax(const T& A, const T& B)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return A || B ? true : false;
		}
		else if constexpr (std::is_same_v<T, FVector2D>)
		{
			return FVector2D(AbsoluteMax(A.X, B.X), AbsoluteMax(A.Y, B.Y));
		}
		else if constexpr (std::is_same_v<T, FVector>)
		{
			return FVector(AbsoluteMax(A.X, B.X), AbsoluteMax(A.Y, B.Y), AbsoluteMax(A.Z, B.Z));
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			return FVector4(AbsoluteMax(A.X, B.X), AbsoluteMax(A.Y, B.Y), AbsoluteMax(A.Z, B.Z), AbsoluteMax(A.W, B.W));
		}
		else if constexpr (std::is_same_v<T, FQuat>)
		{
			return Min(A.Rotator(), B.Rotator()).Quaternion();
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			return FRotator(AbsoluteMax(A.Pitch, B.Pitch), AbsoluteMax(A.Yaw, B.Yaw), AbsoluteMax(A.Roll, B.Roll));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return FTransform(AbsoluteMax(A.GetRotation(), B.GetRotation()), AbsoluteMax(A.GetLocation(), B.GetLocation()), AbsoluteMax(A.GetScale3D(), B.GetScale3D()));
		}
		else if constexpr (std::is_same_v<T, FString> || std::is_same_v<T, FName> || std::is_same_v<T, FSoftClassPath> || std::is_same_v<T, FSoftObjectPath>)
		{
			return Max(B, A);
		}
		else
		{
			return FMath::Max(FMath::Abs(A), FMath::Abs(B));
		}
	}

	template <typename T>
	FORCEINLINE static T Lerp(const T& A, const T& B, const double& W = 0)
	{
		if constexpr (std::is_same_v<T, FQuat>)
		{
			return FQuat::Slerp(A, B, W);
		}
		else if constexpr (std::is_same_v<T, FColor>)
		{
			return FMath::Lerp(A.ReinterpretAsLinear(), B.ReinterpretAsLinear(), W).ToFColor(false);
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			return FRotator(Lerp(A.Pitch, B.Pitch, W), Lerp(A.Yaw, B.Yaw, W), Lerp(A.Roll, B.Roll, W));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return FTransform(Lerp(A.GetRotation(), B.GetRotation(), W).GetNormalized(), Lerp(A.GetLocation(), B.GetLocation(), W), Lerp(A.GetScale3D(), B.GetScale3D(), W));
		}
		else if constexpr (
			std::is_same_v<T, bool> ||
			std::is_same_v<T, FString> ||
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftObjectPath> ||
			std::is_same_v<T, FSoftClassPath>)
		{
			return W > 0.5 ? B : A;
		}
		else
		{
			return FMath::Lerp(A, B, W);
		}
	}

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static T Div(const T& A, const double Divider)
	{
		if constexpr (std::is_same_v<T, FRotator>)
		{
			return FRotator(A.Pitch / Divider, A.Yaw / Divider, A.Roll / Divider);
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return FTransform(Div(A.GetRotation(), Divider).GetNormalized(), Div(A.GetLocation(), Divider), Div(A.GetScale3D(), Divider));
		}
		else if constexpr (
			std::is_same_v<T, bool> ||
			std::is_same_v<T, FString> ||
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftObjectPath> ||
			std::is_same_v<T, FSoftClassPath>)
		{
			return A;
		}
		else
		{
			return A / Divider;
		}
	}

	// SSE optimizations throw unreachable code with FQuat constexpr so we need explicit template impl
	template <typename CompilerSafety = void>
	FORCEINLINE static FQuat Div(const FQuat& A, const double Divider)
	{
		const double R = 1.0 / Divider;
		return FQuat(A.X * R, A.Y * R, A.Z * R, A.W * R).GetNormalized();
	}

	template <typename T>
	FORCEINLINE static T Mult(const T& A, const T& B)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return !A || !B ? false : true;
		}
		else if constexpr (
			std::is_same_v<T, FString> ||
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftObjectPath> ||
			std::is_same_v<T, FSoftClassPath>)
		{
			return A;
		}
		else
		{
			return A * B;
		}
	}

	template <typename T>
	FORCEINLINE static T Copy(const T& A, const T& B) { return B; }

	template <typename T>
	FORCEINLINE static T NoBlend(const T& A, const T& B) { return A; }

	template <typename T>
	FORCEINLINE static T NaiveHash(const T& A, const T& B)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return A || B;
		}
		else if constexpr (std::is_same_v<T, FVector2D>)
		{
			return FVector2D(NaiveHash(A.X, B.X), NaiveHash(A.Y, B.Y));
		}
		else if constexpr (std::is_same_v<T, FVector>)
		{
			return FVector(NaiveHash(A.X, B.X), NaiveHash(A.Y, B.Y), NaiveHash(A.Z, B.Z));
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			return FVector4(NaiveHash(A.X, B.X), NaiveHash(A.Y, B.Y), NaiveHash(A.Z, B.Z), NaiveHash(A.W, B.W));
		}
		else if constexpr (std::is_same_v<T, FColor>)
		{
			return FColor(NaiveHash(A.R, B.R), NaiveHash(A.G, B.G), NaiveHash(A.B, B.B), NaiveHash(A.A, B.A));
		}
		else if constexpr (std::is_same_v<T, FQuat>)
		{
			return NaiveHash(A.Rotator(), B.Rotator()).Quaternion();
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			return FRotator(NaiveHash(A.Pitch, B.Pitch), NaiveHash(A.Yaw, B.Yaw), NaiveHash(A.Roll, B.Roll));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return FTransform(NaiveHash(A.GetRotation(), B.GetRotation()), NaiveHash(A.GetLocation(), B.GetLocation()), NaiveHash(A.GetScale3D(), B.GetScale3D()));
		}
		else if constexpr (std::is_same_v<T, FString>)
		{
			return FString::Printf(TEXT("%d"), NaiveHash(GetTypeHash(A), GetTypeHash(B)));
		}
		else if constexpr (
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftClassPath> ||
			std::is_same_v<T, FSoftObjectPath>)
		{
			return T(NaiveHash(A.ToString(), B.ToString()));
		}
		else
		{
			return static_cast<T>(HashCombineFast(GetTypeHash(A), GetTypeHash(B)));
		}
	}

	template <typename T>
	FORCEINLINE static T NaiveUnsignedHash(const T& A, const T& B)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return A || B;
		}
		else if constexpr (std::is_same_v<T, FVector2D>)
		{
			return FVector2D(NaiveUnsignedHash(A.X, B.X), NaiveUnsignedHash(A.Y, B.Y));
		}
		else if constexpr (std::is_same_v<T, FVector>)
		{
			return FVector(NaiveUnsignedHash(A.X, B.X), NaiveUnsignedHash(A.Y, B.Y), NaiveUnsignedHash(A.Z, B.Z));
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			return FVector4(NaiveUnsignedHash(A.X, B.X), NaiveUnsignedHash(A.Y, B.Y), NaiveUnsignedHash(A.Z, B.Z), NaiveUnsignedHash(A.W, B.W));
		}
		else if constexpr (std::is_same_v<T, FColor>)
		{
			return FColor(NaiveUnsignedHash(A.R, B.R), NaiveUnsignedHash(A.G, B.G), NaiveUnsignedHash(A.B, B.B), NaiveUnsignedHash(A.A, B.A));
		}
		else if constexpr (std::is_same_v<T, FQuat>)
		{
			return NaiveUnsignedHash(A.Rotator(), B.Rotator()).Quaternion();
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			return FRotator(NaiveUnsignedHash(A.Pitch, B.Pitch), NaiveUnsignedHash(A.Yaw, B.Yaw), NaiveUnsignedHash(A.Roll, B.Roll));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return FTransform(NaiveUnsignedHash(A.GetRotation(), B.GetRotation()), NaiveUnsignedHash(A.GetLocation(), B.GetLocation()), NaiveUnsignedHash(A.GetScale3D(), B.GetScale3D()));
		}
		else if constexpr (std::is_same_v<T, FString>)
		{
			return FString::Printf(TEXT("%d"), NaiveUnsignedHash(GetTypeHash(A), GetTypeHash(B)));
		}
		else if constexpr (
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftClassPath> ||
			std::is_same_v<T, FSoftObjectPath>)
		{
			return T(NaiveUnsignedHash(A.ToString(), B.ToString()));
		}
		else
		{
			return static_cast<T>(GetTypeHash(PCGEx::H64U(GetTypeHash(A), GetTypeHash(B))));
		}
	}

#pragma endregion

#pragma region Components

	template <typename T>
	FORCEINLINE static double GetComponent(const T& A, const int32 Index)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return A ? 1 : 0;
		}
		else if constexpr (
			std::is_same_v<T, FVector2D> ||
			std::is_same_v<T, FVector> ||
			std::is_same_v<T, FVector4>)
		{
			return A[Index];
		}
		else if constexpr (
			std::is_same_v<T, FQuat> ||
			std::is_same_v<T, FRotator>)
		{
			return A.Euler()[Index];
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return A.GetLocation()[Index];
		}
		else if constexpr (
			std::is_same_v<T, FString> ||
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftClassPath> ||
			std::is_same_v<T, FSoftObjectPath>)
		{
			return 0;
		}
		else
		{
			return A;
		}
	}

	template <typename T>
	FORCEINLINE static void SetComponent(T& A, const int32 Index, const double InValue)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			A = InValue > 0;
		}
		else if constexpr (
			std::is_same_v<T, FVector2D> ||
			std::is_same_v<T, FVector> ||
			std::is_same_v<T, FVector4>)
		{
			A[Index] = InValue;
		}
		else if constexpr (std::is_same_v<T, FQuat>)
		{
			FVector Euler = A.Euler();
			SetComponent(Euler, Index, InValue);
			A = FQuat::MakeFromEuler(Euler);
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			FVector Euler = A.Euler();
			SetComponent(Euler, Index, InValue);
			A = FRotator::MakeFromEuler(Euler);
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			FVector Location = A.GetLocation();
			SetComponent(Location, Index, InValue);
			A.SetLocation(Location);
		}
		else if constexpr (
			std::is_same_v<T, FString> ||
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftClassPath> ||
			std::is_same_v<T, FSoftObjectPath>)
		{
		}
		else
		{
			A = InValue;
		}
	}

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

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static T DblMult(const T& A, const double M)
	{
		if constexpr (std::is_same_v<T, FQuat>)
		{
			return (A.Rotator() * M).Quaternion();
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return FTransform((A.Rotator() * M).Quaternion(), A.GetLocation() * M, A.GetScale3D() * M);
		}
		else if constexpr (
			std::is_same_v<T, FString> ||
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftClassPath> ||
			std::is_same_v<T, FSoftObjectPath>)
		{
			return A;
		}
		else
		{
			return A * M;
		}
	}

	// For some reason if constexpr (std::is_same_v<T, bool>) refused to compile in the branching above
	template <typename CompilerSafety = void>
	FORCEINLINE static bool DblMult(const bool& A, const double M)
	{
		return M > 0 ? A : false;
	}

#pragma endregion

	template <typename T>
	FORCEINLINE void ReverseRange(TArray<T>& InArray, int32 Start, int32 End)
	{
		while (Start < End)
		{
			InArray.Swap(Start, End);
			Start++;
			End--;
		}
	}

	template <typename T, EPCGExIndexSafety Safety = EPCGExIndexSafety::Ignore>
	FORCEINLINE static T SanitizeIndex(const T& Index, const T& MaxIndex)
	{
		if constexpr (Safety == EPCGExIndexSafety::Yoyo)
		{
			const T L = 2 * MaxIndex;
			const T C = Index % L;
			return C <= MaxIndex ? C : L - C;
		}
		else if constexpr (Safety == EPCGExIndexSafety::Tile)
		{
			return PCGExMath::Tile(Index, 0, MaxIndex);
		}
		else if constexpr (Safety == EPCGExIndexSafety::Clamp)
		{
			return FMath::Clamp(Index, 0, MaxIndex);
		}
		else
		{
			return Index < 0 || Index > MaxIndex ? -1 : Index;
		}
	}

	template <typename T>
	FORCEINLINE static T SanitizeIndex(const T& Index, const T& MaxIndex, const EPCGExIndexSafety Method)
	{
		switch (Method)
		{
		default:
		case EPCGExIndexSafety::Ignore: return SanitizeIndex<T, EPCGExIndexSafety::Ignore>(Index, MaxIndex);
		case EPCGExIndexSafety::Tile: return SanitizeIndex<T, EPCGExIndexSafety::Tile>(Index, MaxIndex);
		case EPCGExIndexSafety::Clamp: return SanitizeIndex<T, EPCGExIndexSafety::Clamp>(Index, MaxIndex);
		case EPCGExIndexSafety::Yoyo: return SanitizeIndex<T, EPCGExIndexSafety::Yoyo>(Index, MaxIndex);
		}
	}

	template <const EPCGExAxis Dir = EPCGExAxis::Forward>
	FORCEINLINE static FVector GetDirection(const FQuat& Quat)
	{
		if constexpr (Dir == EPCGExAxis::Backward) { return Quat.GetForwardVector() * -1; }
		else if constexpr (Dir == EPCGExAxis::Right) { return Quat.GetRightVector(); }
		else if constexpr (Dir == EPCGExAxis::Left) { return Quat.GetRightVector() * -1; }
		else if constexpr (Dir == EPCGExAxis::Up) { return Quat.GetUpVector(); }
		else if constexpr (Dir == EPCGExAxis::Down) { return Quat.GetUpVector() * -1; }
		else { return Quat.GetForwardVector(); }
	}

	FVector GetDirection(const FQuat& Quat, const EPCGExAxis Dir);
	FVector GetDirection(const EPCGExAxis Dir);

	FQuat MakeDirection(const EPCGExAxis Dir, const FVector& InForward);
	FQuat MakeDirection(const EPCGExAxis Dir, const FVector& InForward, const FVector& InUp);

	FVector GetNormal(const FVector& A, const FVector& B, const FVector& C);
	FVector GetNormalUp(const FVector& A, const FVector& B, const FVector& Up);

	FTransform MakeLookAtTransform(const FVector& LookAt, const FVector& LookUp, const EPCGExAxisAlign AlignAxis);

	double GetAngle(const FVector& A, const FVector& B);

	// Expects normalized vectors
	double GetRadiansBetweenVectors(const FVector& A, const FVector& B, const FVector& UpVector = FVector::UpVector);
	double GetDegreesBetweenVectors(const FVector& A, const FVector& B, const FVector& UpVector = FVector::UpVector);

	void CheckConvex(const FVector& A, const FVector& B, const FVector& C, bool& bIsConvex, int32& OutSign, const FVector& UpVector = FVector::UpVector);
	FBox ScaledBox(const FBox& InBox, const FVector& InScale);
	bool IsDirectionWithinTolerance(const FVector& A, const FVector& B, const FRotator& Limits);

	template <typename T>
	FORCEINLINE static void TypeMinMax(T& Min, T& Max)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			Min = false;
			Max = true;
		}
		else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>)
		{
			Min = TNumericLimits<T>::Max();
			Max = TNumericLimits<T>::Min();
		}
		else if constexpr (std::is_same_v<T, FVector2D>)
		{
			Min = FVector2D(MAX_dbl);
			Max = FVector2D(MIN_dbl_neg);
		}
		else if constexpr (std::is_same_v<T, FVector>)
		{
			Min = FVector(MAX_dbl);
			Max = FVector(MIN_dbl_neg);
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			Min = FVector4(MAX_dbl, MAX_dbl, MAX_dbl, MAX_dbl);
			Max = FVector4(MIN_dbl_neg, MIN_dbl_neg, MIN_dbl_neg, MIN_dbl_neg);
		}
		else if constexpr (std::is_same_v<T, FQuat>)
		{
			Min = FRotator(MAX_dbl, MAX_dbl, MAX_dbl).Quaternion();
			Max = FRotator(MIN_dbl_neg, MIN_dbl_neg, MIN_dbl_neg).Quaternion();
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			Min = FRotator(MAX_dbl, MAX_dbl, MAX_dbl);
			Max = FRotator(MIN_dbl_neg, MIN_dbl_neg, MIN_dbl_neg);
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			Min = FTransform(FRotator(MAX_dbl, MAX_dbl, MAX_dbl).Quaternion(), FVector(MAX_dbl), FVector(MAX_dbl));
			Max = FTransform(FRotator(MIN_dbl_neg, MIN_dbl_neg, MIN_dbl_neg).Quaternion(), FVector(MIN_dbl_neg), FVector(MIN_dbl_neg));
		}
		else
		{
			Min = T{};
			Max = T{};
		}
	}

	double GetArcLength(const double R, const double StartAngleRadians, const double EndAngleRadians);

	/** Distance from C to AB */
	double GetPerpendicularDistance(const FVector& A, const FVector& B, const FVector& C);


#pragma region Spatialized distances

	template <EPCGExDistance Mode>
	static FVector GetSpatializedCenter(
		const FPCGPoint& FromPoint,
		const FVector& FromCenter,
		const FVector& ToCenter)
	{
		if constexpr (Mode == EPCGExDistance::None)
		{
			return FVector::OneVector;
		}
		else if constexpr (Mode == EPCGExDistance::SphereBounds)
		{
			FVector Dir = ToCenter - FromCenter;
			Dir.Normalize();

			return FromCenter + Dir * FromPoint.GetScaledExtents().Length();
		}
		else if constexpr (Mode == EPCGExDistance::BoxBounds)
		{
			const FVector LocalTargetCenter = FromPoint.Transform.InverseTransformPosition(ToCenter);

			const double DistanceSquared = ComputeSquaredDistanceFromBoxToPoint(FromPoint.BoundsMin, FromPoint.BoundsMax, LocalTargetCenter);

			FVector Dir = -LocalTargetCenter;
			Dir.Normalize();

			const FVector LocalClosestPoint = LocalTargetCenter + Dir * FMath::Sqrt(DistanceSquared);

			return FromPoint.Transform.TransformPosition(LocalClosestPoint);
		}
		else
		{
			return FromCenter;
		}
	}

#pragma endregion
}
