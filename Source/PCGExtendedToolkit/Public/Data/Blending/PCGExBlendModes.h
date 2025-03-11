// Copyright 2025 Timothé Lapetite and contributors
// * 24/02/25 Omer Salomon Changed - Div(const FQuat& A, const double Divider) now converts FQuat to FRotator and calls Div on that.
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExH.h"

namespace PCGExBlend
{
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
			std::is_same_v<T, float> ||
			std::is_same_v<T, double> ||
			std::is_same_v<T, int32> ||
			std::is_same_v<T, int64> ||
			std::is_same_v<T, FVector2D> ||
			std::is_same_v<T, FVector>)
		{
			return A * B;
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			return FRotator(A.Pitch * B.Pitch, A.Yaw * B.Yaw, A.Roll * B.Roll);
		}
		else if constexpr (std::is_same_v<T, FQuat>)
		{
			return (A * B).GetNormalized();
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			return FVector4(A.X * B.X, A.Y * B.Y, A.Z * B.Z, A.W * B.W);
		}
		else
		{
			return A; // Unsupported fallback
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
}
