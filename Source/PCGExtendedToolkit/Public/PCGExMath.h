// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"

namespace PCGExMath
{
	inline static double ConvertStringToDouble(const FString& StringToConvert)
	{
		const TCHAR* CharArray = *StringToConvert;
		const double Result = FCString::Atod(CharArray);
		return FMath::IsNaN(Result) ? 0 : Result;
	}

	// Remap function
	inline static double Remap(const double InBase, const double InMin, const double InMax, const double OutMin = 0, const double OutMax = 1)
	{
		return FMath::Lerp(OutMin, OutMax, (InBase - InMin) / (InMax - InMin));
	}

	inline static FVector CWWrap(const FVector& InValue, const FVector& Min, const FVector& Max)
	{
		return FVector(
			FMath::Wrap(InValue.X, Min.X, Max.X),
			FMath::Wrap(InValue.Y, Min.Y, Max.Y),
			FMath::Wrap(InValue.Z, Min.Z, Max.Z));
	}

#pragma region Component Wise MIN

	template <typename T, typename dummy = void>
	inline static T CWMin(const T& A, const T& B) { return FMath::Max(A, B); }

	template <typename dummy = void>
	inline static bool CWMin(bool& A, const bool& B) { return B ? true : A; }

	template <typename dummy = void>
	inline static FVector2D CWMin(const FVector2D& A, const FVector2D& B)
	{
		return FVector2D(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y));
	}

	template <typename dummy = void>
	inline static FVector CWMin(const FVector& A, const FVector& B)
	{
		return FVector(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y),
			FMath::Min(A.Z, B.Z));
	}

	template <typename dummy = void>
	inline static FVector4 CWMin(const FVector4& A, const FVector4& B)
	{
		return FVector4(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y),
			FMath::Min(A.Z, B.Z),
			FMath::Min(A.W, B.W));
	}

	template <typename dummy = void>
	inline static FRotator CWMin(const FRotator& A, const FRotator& B)
	{
		return FRotator(
			FMath::Min(A.Pitch, B.Pitch),
			FMath::Min(A.Yaw, B.Yaw),
			FMath::Min(A.Roll, B.Roll));
	}

	template <typename dummy = void>
	inline static FQuat CWMin(const FQuat& A, const FQuat& B)
	{
		return CWMin(A.Rotator(), B.Rotator()).Quaternion();
	}

	template <typename dummy = void>
	inline static FTransform CWMin(const FTransform& A, const FTransform& B)
	{
		return FTransform(
			CWMin(A.GetRotation(), B.GetRotation()),
			CWMin(A.GetLocation(), B.GetLocation()),
			CWMin(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename dummy = void>
	inline static FName CWMin(const FName& A, const FName& B) { return A.ToString() > B.ToString() ? B : A; }

	template <typename dummy = void>
	inline static FString CWMin(const FString& A, const FString& B) { return A > B ? B : A; }

#pragma endregion

#pragma region Component Wise MAX

	template <typename T, typename dummy = void>
	inline static T CWMax(const T& A, const T& B) { return FMath::Max(A, B); }

	template <typename dummy = void>
	inline static bool CWMax(bool& A, const bool& B) { return B ? true : A; }

	template <typename dummy = void>
	inline static FVector2D CWMax(const FVector2D& A, const FVector2D& B)
	{
		return FVector2D(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y));
	}

	template <typename dummy = void>
	inline static FVector CWMax(const FVector& A, const FVector& B)
	{
		return FVector(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y),
			FMath::Max(A.Z, B.Z));
	}

	template <typename dummy = void>
	inline static FVector4 CWMax(const FVector4& A, const FVector4& B)
	{
		return FVector4(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y),
			FMath::Max(A.Z, B.Z),
			FMath::Max(A.W, B.W));
	}

	template <typename dummy = void>
	inline static FRotator CWMax(const FRotator& A, const FRotator& B)
	{
		return FRotator(
			FMath::Max(A.Pitch, B.Pitch),
			FMath::Max(A.Yaw, B.Yaw),
			FMath::Max(A.Roll, B.Roll));
	}

	template <typename dummy = void>
	inline static FQuat CWMax(const FQuat& A, const FQuat& B)
	{
		return CWMax(A.Rotator(), B.Rotator()).Quaternion();
	}

	template <typename dummy = void>
	inline static FTransform CWMax(const FTransform& A, const FTransform& B)
	{
		return FTransform(
			CWMax(A.GetRotation(), B.GetRotation()),
			CWMax(A.GetLocation(), B.GetLocation()),
			CWMax(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename dummy = void>
	inline static FName CWMax(const FName& A, const FName& B) { return A.ToString() > B.ToString() ? A : B; }

	template <typename dummy = void>
	inline static FString CWMax(const FString& A, const FString& B) { return A > B ? A : B; }

#pragma endregion

#pragma region Lerp

	template <typename T, typename dummy = void>
	inline static T Lerp(const T& A, const T& B, const double Alpha) { return FMath::Lerp(A, B, Alpha); }

	template <typename dummy = void>
	inline static FTransform Lerp(const FTransform& A, const FTransform& B, const double Alpha)
	{
		return FTransform(
			Lerp(A.GetRotation(), B.GetRotation(), Alpha),
			Lerp(A.GetLocation(), B.GetLocation(), Alpha),
			Lerp(A.GetScale3D(), B.GetScale3D(), Alpha));
	}

	template <typename dummy = void>
	inline static FQuat Lerp(const FQuat& A, const FQuat& B, const double Alpha) { return FQuat::Slerp(A, B, Alpha); }

	template <typename dummy = void>
	inline static FName Lerp(const FName& A, const FName& B, const double Alpha) { return Alpha > 0.5 ? B : A; }

	template <typename dummy = void>
	inline static FString Lerp(const FString& A, const FString& B, const double Alpha) { return Alpha > 0.5 ? B : A; }

	inline static void Lerp(const FPCGPoint& A, const FPCGPoint& B, FPCGPoint& Out, const double Alpha)
	{
#define PCGEX_POINT_PROPERTY_LERP(_NAME) Out._NAME = Lerp(A._NAME, B._NAME, Alpha);
		PCGEX_FOREACH_GETSET_POINTPROPERTY(PCGEX_POINT_PROPERTY_LERP)
#undef PCGEX_POINT_PROPERTY_LERP
	}

#pragma endregion

#pragma region ADD

	template <typename T, typename dummy = void>
	inline static T Add(const T& A, const T& B) { return A + B; }

	template <typename dummy = void>
	inline static FName Add(const FName& A, const FName& B) { return A; }

	template <typename dummy = void>
	inline static FString Add(const FString& A, const FString& B) { return A; }

	template <typename dummy = void>
	inline static bool Add(const bool& A, const bool& B) { return B ? true : A; }

#pragma endregion

#pragma region Component Wise DIV

	template <typename T, typename dummy = void>
	inline static T CWDivide(const T& A, const double Divider) { return A / Divider; }

	template <typename dummy = void>
	inline static FRotator CWDivide(const FRotator& A, const double Divider)
	{
		return FRotator(
			A.Pitch / Divider,
			A.Yaw / Divider,
			A.Roll / Divider);
	}

#pragma endregion

	template <typename T>
	inline static int SignPlus(const T& InValue)
	{
		int sign = FMath::Sign(InValue);
		return sign == 0 ? 1 : sign;
	}

	template <typename T>
	inline static int SignMinus(const T& InValue)
	{
		int sign = FMath::Sign(InValue);
		return sign == 0 ? -1 : sign;
	}

	static FBox ConeBox(const FVector& Center, const FVector& ConeDirection, double Size)
	{
		FVector Dir = ConeDirection.GetSafeNormal();
		const FVector U = FVector::CrossProduct(Dir, Dir + FVector(0.1)).GetSafeNormal();
		const FVector V = FVector::CrossProduct(Dir, Dir + FVector(-0.1)).GetSafeNormal();

		FBox Box = FBoxCenterAndExtent(Center, FVector(0.0001)).GetBox();
		Box += Center + Dir * Size;
		Box += Center + U * Size;
		Box += Center + V * Size;

		//UE_LOG(LogTemp, Warning, TEXT("Box Min X:%f, Y:%f, Z:%f | Max X:%f, Y:%f, Z:%f"), Box.Min.X, Box.Min.Y, Box.Min.Z, Box.Max.X, Box.Max.Y, Box.Max.Z);

		return Box;
	}
}
