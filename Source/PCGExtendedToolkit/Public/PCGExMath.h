﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGEx.h"
#include "Details/PCGExDetailsAxis.h"
#include "PCGExMath.generated.h"

namespace PCGExData
{
	struct FProxyPoint;
	class FFacade;
	struct FConstPoint;

	template <typename T>
	class TBuffer;
}

UENUM()
enum class EPCGExIndexSafety : uint8
{
	Ignore = 0 UMETA(DisplayName = "Ignore", Tooltip="Out of bounds indices are ignored."),
	Tile   = 1 UMETA(DisplayName = "Tile", Tooltip="Out of bounds indices are tiled."),
	Clamp  = 2 UMETA(DisplayName = "Clamp", Tooltip="Out of bounds indices are clamped."),
	Yoyo   = 3 UMETA(DisplayName = "Yoyo", Tooltip="Out of bounds indices are mirrored and back."),
};

UENUM()
enum class EPCGExTruncateMode : uint8
{
	None  = 0 UMETA(DisplayName = "None", ToolTip="None"),
	Round = 1 UMETA(DisplayName = "Round", ToolTip="Round"),
	Ceil  = 2 UMETA(DisplayName = "Ceil", ToolTip="Ceil"),
	Floor = 3 UMETA(DisplayName = "Floor", ToolTip="Floor"),
};

namespace PCGExMath
{
	enum class EIntersectionTestMode : uint8
	{
		Loose = 0,
		Strict,
		StrictOnSelfA,
		StrictOnSelfB,
		StrictOnOtherA,
		StrictOnOtherB,
		LooseOnSelf,
		LooseOnSelfA,
		LooseOnSelfB,
		LooseOnOther,
		LooseOnOtherA,
		LooseOnOtherB,
	};

	struct PCGEXTENDEDTOOLKIT_API FClosestPosition
	{
		bool bValid = false;
		int32 Index = -1;
		FVector Origin = FVector::ZeroVector;
		FVector Location = FVector::ZeroVector;
		double DistSquared = MAX_dbl;

		FClosestPosition() = default;
		explicit FClosestPosition(const FVector& InOrigin);
		FClosestPosition(const FVector& InOrigin, const FVector& InLocation);
		FClosestPosition(const FVector& InOrigin, const FVector& InLocation, const int32 InIndex);

		bool Update(const FVector& InLocation);
		bool Update(const FVector& InLocation, const int32 InIndex);

		FVector Direction() const { return (Origin - Location).GetSafeNormal(); }

		friend bool operator<(const FClosestPosition& A, const FClosestPosition& B) { return A.DistSquared < B.DistSquared; }
		friend bool operator>(const FClosestPosition& A, const FClosestPosition& B) { return A.DistSquared > B.DistSquared; }

		operator FVector() const { return Location; }
		operator double() const { return DistSquared; }
		operator bool() const { return bValid; }
	};

	struct PCGEXTENDEDTOOLKIT_API FSegment
	{
		FVector A = FVector::ZeroVector;
		FVector B = FVector::ZeroVector;
		FVector Direction = FVector::ZeroVector;
		FBox Bounds = FBox(ForceInit);

		FSegment() = default;
		FSegment(const FVector& InA, const FVector& InB, const double Expansion = 0);

		double Dot(const FVector& InDirection) const { return FVector::DotProduct(Direction, InDirection); }
		double Dot(const FSegment& InSegment) const { return FVector::DotProduct(Direction, InSegment.Direction); }
		FVector Lerp(const double InLerp) const { return FMath::Lerp(A, B, InLerp); }

		bool FindIntersection(const FVector& A2, const FVector& B2, double SquaredTolerance, FVector& OutSelf, FVector& OutOther, const EIntersectionTestMode Mode = EIntersectionTestMode::Strict) const;
		bool FindIntersection(const FSegment& S, double SquaredTolerance, FVector& OutSelf, FVector& OutOther, const EIntersectionTestMode Mode = EIntersectionTestMode::Strict) const;
	};

	PCGEXTENDEDTOOLKIT_API
	double TruncateDbl(const double Value, const EPCGExTruncateMode Mode);

#pragma region basics

	FORCEINLINE static double DegreesToDot(const double AngleInDegrees)
	{
		return FMath::Cos(FMath::DegreesToRadians(AngleInDegrees));
	}

	PCGEXTENDEDTOOLKIT_API
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

		//UE_LOG(LogPCGEx, Warning, TEXT("Box Min X:%f, Y:%f, Z:%f | Max X:%f, Y:%f, Z:%f"), Box.Min.X, Box.Min.Y, Box.Min.Z, Box.Max.X, Box.Max.Y, Box.Max.Z);

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

	PCGEXTENDEDTOOLKIT_API
	FVector SafeLinePlaneIntersection(
		const FVector& Pt1, const FVector& Pt2,
		const FVector& PlaneOrigin, const FVector& PlaneNormal,
		bool& bIntersect);


	PCGEXTENDEDTOOLKIT_API
	bool SphereOverlap(const FSphere& S1, const FSphere& S2, double& OutOverlap);
	PCGEXTENDEDTOOLKIT_API
	bool SphereOverlap(const FBoxSphereBounds& S1, const FBoxSphereBounds& S2, double& OutOverlap);


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

	PCGEXTENDEDTOOLKIT_API
	FVector GetDirection(const FQuat& Quat, const EPCGExAxis Dir);
	PCGEXTENDEDTOOLKIT_API
	FVector GetDirection(const EPCGExAxis Dir);
	PCGEXTENDEDTOOLKIT_API
	FTransform GetIdentity(const EPCGExAxisOrder Order);

	PCGEXTENDEDTOOLKIT_API
	void Swizzle(FVector& Vector, const EPCGExAxisOrder Order);

	PCGEXTENDEDTOOLKIT_API
	void Swizzle(FVector& Vector, const int32 (&Order)[3]);

	PCGEXTENDEDTOOLKIT_API
	FQuat MakeDirection(const EPCGExAxis Dir, const FVector& InForward);
	PCGEXTENDEDTOOLKIT_API
	FQuat MakeDirection(const EPCGExAxis Dir, const FVector& InForward, const FVector& InUp);

	PCGEXTENDEDTOOLKIT_API
	FVector GetNormal(const FVector& A, const FVector& B, const FVector& C);
	PCGEXTENDEDTOOLKIT_API
	FVector GetNormalUp(const FVector& A, const FVector& B, const FVector& Up);

	PCGEXTENDEDTOOLKIT_API
	FTransform MakeLookAtTransform(const FVector& LookAt, const FVector& LookUp, const EPCGExAxisAlign AlignAxis);

	PCGEXTENDEDTOOLKIT_API
	double GetAngle(const FVector& A, const FVector& B);

	// Expects normalized vectors
	PCGEXTENDEDTOOLKIT_API
	double GetRadiansBetweenVectors(const FVector& A, const FVector& B, const FVector& UpVector = FVector::UpVector);
	PCGEXTENDEDTOOLKIT_API
	double GetRadiansBetweenVectors(const FVector2D& A, const FVector2D& B);
	PCGEXTENDEDTOOLKIT_API
	double GetDegreesBetweenVectors(const FVector& A, const FVector& B, const FVector& UpVector = FVector::UpVector);

	PCGEXTENDEDTOOLKIT_API
	void CheckConvex(const FVector& A, const FVector& B, const FVector& C, bool& bIsConvex, int32& OutSign, const FVector& UpVector = FVector::UpVector);
	PCGEXTENDEDTOOLKIT_API
	FBox ScaledBox(const FBox& InBox, const FVector& InScale);
	PCGEXTENDEDTOOLKIT_API
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

	PCGEXTENDEDTOOLKIT_API
	double GetArcLength(const double R, const double StartAngleRadians, const double EndAngleRadians);

	/** Distance from C to AB */
	PCGEXTENDEDTOOLKIT_API
	double GetPerpendicularDistance(const FVector& A, const FVector& B, const FVector& C);
}
