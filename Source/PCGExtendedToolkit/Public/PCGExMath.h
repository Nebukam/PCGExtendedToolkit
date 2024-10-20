// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"

#include "PCGExMath.generated.h"

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
	template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds>
	FORCEINLINE static FBox GetLocalBounds(const FPCGPoint& Point)
	{
		if constexpr (S == EPCGExPointBoundsSource::ScaledBounds)
		{
			const FBox LocalBounds = Point.GetLocalBounds();
			const FVector Scale = Point.Transform.GetScale3D();
			return FBox(LocalBounds.Min * Scale, LocalBounds.Max * Scale);
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
	FORCEINLINE static FBox GetLocalBounds(const FPCGPoint* Point)
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

	FORCEINLINE static FBox GetLocalBounds(const FPCGPoint& Point, const EPCGExPointBoundsSource Source)
	{
		if (Source == EPCGExPointBoundsSource::ScaledBounds) { return GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(Point); }
		if (Source == EPCGExPointBoundsSource::Bounds) { return GetLocalBounds<EPCGExPointBoundsSource::Bounds>(Point); }
		if (Source == EPCGExPointBoundsSource::DensityBounds) { return GetLocalBounds<EPCGExPointBoundsSource::DensityBounds>(Point); }
		return FBox(FVector::OneVector * -1, FVector::OneVector);
	}

	FORCEINLINE static FBox GetLocalBounds(const FPCGPoint* Point, const EPCGExPointBoundsSource Source)
	{
		if (Source == EPCGExPointBoundsSource::ScaledBounds) { return GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(Point); }
		if (Source == EPCGExPointBoundsSource::Bounds) { return GetLocalBounds<EPCGExPointBoundsSource::Bounds>(Point); }
		if (Source == EPCGExPointBoundsSource::DensityBounds) { return GetLocalBounds<EPCGExPointBoundsSource::DensityBounds>(Point); }
		return FBox(FVector::OneVector * -1, FVector::OneVector);
	}

#pragma region basics

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

	template <typename T>
	FORCEINLINE static T Tile(const T Value, const T Min, const T Max)
	{
		if constexpr (std::is_same_v<T, FVector2D>)
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
			if (FMath::IsWithin(Value, Min, Max)) { return Value; }

			T OutValue = Value;
			T Range = Max - Min + 1;

			OutValue = FMath::Fmod(static_cast<double>(OutValue - Min), static_cast<double>(Range));
			if (OutValue < 0) { OutValue += Range; }

			return OutValue + Min;
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

	FORCEINLINE bool SphereOverlap(const FSphere& S1, const FSphere& S2, double& OutOverlap)
	{
		OutOverlap = (S1.W + S2.W) - FVector::Dist(S1.Center, S2.Center);
		return OutOverlap > 0;
	}

	FORCEINLINE bool SphereOverlap(const FBoxSphereBounds& S1, const FBoxSphereBounds& S2, double& OutOverlap)
	{
		return SphereOverlap(S1.GetSphere(), S2.GetSphere(), OutOverlap);
	}

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
			return FTransform(WeightedSub(A.GetRotation(), B.GetRotation(), W), WeightedSub(A.GetLocation(), B.GetLocation(), W), WeightedSub(A.GetScale3D(), B.GetScale3D(), W));
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
			return FTransform(Lerp(A.GetRotation(), B.GetRotation(), W), Lerp(A.GetLocation(), B.GetLocation(), W), Lerp(A.GetScale3D(), B.GetScale3D(), W));
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
			return FTransform(Div(A.GetRotation(), Divider), Div(A.GetLocation(), Divider), Div(A.GetScale3D(), Divider));
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
		return Div(A.Rotator(), Divider).Quaternion();
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
			return (Index < 0 || Index > MaxIndex) ? -1 : Index;
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

	FORCEINLINE static FVector GetDirection(const FQuat& Quat, const EPCGExAxis Dir)
	{
		switch (Dir)
		{
		default:
		case EPCGExAxis::Forward: return GetDirection<EPCGExAxis::Forward>(Quat);
		case EPCGExAxis::Backward: return GetDirection<EPCGExAxis::Backward>(Quat);
		case EPCGExAxis::Right: return GetDirection<EPCGExAxis::Right>(Quat);
		case EPCGExAxis::Left: return GetDirection<EPCGExAxis::Left>(Quat);
		case EPCGExAxis::Up: return GetDirection<EPCGExAxis::Up>(Quat);
		case EPCGExAxis::Down: return GetDirection<EPCGExAxis::Down>(Quat);
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
