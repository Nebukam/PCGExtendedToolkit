// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"

namespace PCGExMath
{
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

	static FVector CWWrap(const FVector& InValue, const FVector& Min, const FVector& Max)
	{
		return FVector(
			FMath::Wrap(InValue.X, Min.X, Max.X),
			FMath::Wrap(InValue.Y, Min.Y, Max.Y),
			FMath::Wrap(InValue.Z, Min.Z, Max.Z));
	}

	// MIN

	template <typename T, typename dummy = void>
	static void CWMin(T& InBase, const T& Other) { InBase = FMath::Min(InBase, Other); }

	template <typename dummy = void>
	static void CWMin(bool& InBase, const bool& Other) { InBase = !Other ? false : InBase; }
	
	template <typename dummy = void>
	static void CWMin(FVector2D& InBase, const FVector2D& Other)
	{
		InBase.X = FMath::Min(InBase.X, Other.X);
		InBase.Y = FMath::Min(InBase.Y, Other.Y);
	}

	template <typename dummy = void>
	static void CWMin(FVector& InBase, const FVector& Other)
	{
		InBase.X = FMath::Min(InBase.X, Other.X);
		InBase.Y = FMath::Min(InBase.Y, Other.Y);
		InBase.Z = FMath::Min(InBase.Z, Other.Z);
	}

	template <typename dummy = void>
	static void CWMin(FVector4& InBase, const FVector4& Other)
	{
		InBase.X = FMath::Min(InBase.X, Other.X);
		InBase.Y = FMath::Min(InBase.Y, Other.Y);
		InBase.Z = FMath::Min(InBase.Z, Other.Z);
		InBase.W = FMath::Min(InBase.W, Other.W);
	}

	template <typename dummy = void>
	static void CWMin(FRotator& InBase, const FRotator& Other)
	{
		InBase.Pitch = FMath::Min(InBase.Pitch, Other.Pitch);
		InBase.Roll = FMath::Min(InBase.Roll, Other.Roll);
		InBase.Yaw = FMath::Min(InBase.Yaw, Other.Yaw);
	}
	
	template <typename dummy = void>
	static void CWMin(FQuat& InBase, const FQuat& Other)
	{
		FRotator A = InBase.Rotator();
		CWMin(A, Other.Rotator());
		InBase = A.Quaternion();
	}

	template <typename dummy = void>
	static void CWMin(FTransform& InBase, const FTransform& Other)
	{
		FVector Pos = InBase.GetLocation();
		FQuat Rot = InBase.GetRotation();
		FVector Scale = InBase.GetScale3D();
		CWMin(Pos, Other.GetLocation());
		CWMin(Rot, Other.GetRotation());
		CWMin(Scale, Other.GetScale3D());
		InBase.SetLocation(Pos);
		InBase.SetRotation(Rot);
		InBase.SetScale3D(Scale);
	}

	template <typename dummy = void>
	static void CWMin(FName& InBase, const FName& Other)
	{
	}

	template <typename dummy = void>
	static void CWMin(FString& InBase, const FString& Other)
	{
	}

	// MAX

	template <typename T, typename dummy = void>
	static void CWMax(T& InBase, const T& Other) { InBase = FMath::Max(InBase, Other); }

	template <typename dummy = void>
	static void CWMax(bool& InBase, const bool& Other) { InBase = Other ? true : InBase; }
	
	template <typename dummy = void>
	static void CWMax(FVector2D& InBase, const FVector2D& Other)
	{
		InBase.X = FMath::Max(InBase.X, Other.X);
		InBase.Y = FMath::Max(InBase.Y, Other.Y);
	}

	template <typename dummy = void>
	static void CWMax(FVector& InBase, const FVector& Other)
	{
		InBase.X = FMath::Max(InBase.X, Other.X);
		InBase.Y = FMath::Max(InBase.Y, Other.Y);
		InBase.Z = FMath::Max(InBase.Z, Other.Z);
	}

	template <typename dummy = void>
	static void CWMax(FVector4& InBase, const FVector4& Other)
	{
		InBase.X = FMath::Max(InBase.X, Other.X);
		InBase.Y = FMath::Max(InBase.Y, Other.Y);
		InBase.Z = FMath::Max(InBase.Z, Other.Z);
		InBase.W = FMath::Max(InBase.W, Other.W);
	}

	template <typename dummy = void>
	static void CWMax(FRotator& InBase, const FRotator& Other)
	{
		InBase.Pitch = FMath::Max(InBase.Pitch, Other.Pitch);
		InBase.Roll = FMath::Max(InBase.Roll, Other.Roll);
		InBase.Yaw = FMath::Max(InBase.Yaw, Other.Yaw);
	}
	
	template <typename dummy = void>
	static void CWMax(FQuat& InBase, const FQuat& Other)
	{
		FRotator A = InBase.Rotator();
		CWMax(A, Other.Rotator());
		InBase = A.Quaternion();
	}

	template <typename dummy = void>
	static void CWMax(FTransform& InBase, const FTransform& Other)
	{
		FVector Pos = InBase.GetLocation();
		FQuat Rot = InBase.GetRotation();
		FVector Scale = InBase.GetScale3D();
		CWMax(Pos, Other.GetLocation());
		CWMax(Rot, Other.GetRotation());
		CWMax(Scale, Other.GetScale3D());
		InBase.SetLocation(Pos);
		InBase.SetRotation(Rot);
		InBase.SetScale3D(Scale);
	}
	
	template <typename dummy = void>
	static void CWMax(FName& InBase, const FName& Other) { InBase = Other; }

	template <typename dummy = void>
	static void CWMax(FString& InBase, const FString& Other) { InBase = Other; }

	// Lerp

	template <typename T, typename dummy = void>
	static void LerpTo(T& From, const T& To, const double Alpha) { From = FMath::Lerp(From, To, Alpha); }

	template <typename dummy = void>
	static void LerpTo(FTransform& From, const FTransform& To, const double Alpha)
	{
		From.SetLocation(FMath::Lerp(From.GetLocation(), To.GetLocation(), Alpha));
		From.SetRotation(FQuat::Slerp(From.GetRotation(), To.GetRotation(), Alpha));
		From.SetScale3D(FMath::Lerp(From.GetScale3D(), To.GetScale3D(), Alpha));
	}

	template <typename dummy = void>
	static void LerpTo(FName& From, const FName& To, const double Alpha)
	{
	}

	template <typename dummy = void>
	static void LerpTo(FString& From, const FString& To, const double Alpha)
	{
	}

	template <typename dummy = void>
	static void LerpTo(FQuat& From, const FQuat& To, const double Alpha)
	{
		From = FQuat::Slerp(From, To, Alpha);
	}

	template <typename T, typename dummy = void>
	static T Lerp(const T& From, const T& To, const double Alpha) { return FMath::Lerp(From, To, Alpha); }

	template <typename dummy = void>
	static FTransform Lerp(const FTransform& From, const FTransform& To, const double Alpha)
	{
		FTransform Result = From;
		Result.SetLocation(FMath::Lerp(From.GetLocation(), To.GetLocation(), Alpha));
		Result.SetRotation(FQuat::Slerp(From.GetRotation(), To.GetRotation(), Alpha));
		Result.SetScale3D(FMath::Lerp(From.GetScale3D(), To.GetScale3D(), Alpha));
		return Result;
	}

	template <typename dummy = void>
	static FQuat Lerp(const FQuat& From, const FQuat& To, const double Alpha) { return FQuat::Slerp(From, To, Alpha); }

	template <typename dummy = void>
	static FName Lerp(const FName& From, const FName& To, const double Alpha) { return Alpha > 0.5 ? To : From; }

	template <typename dummy = void>
	static FString Lerp(const FString& From, const FString& To, const double Alpha) { return Alpha > 0.5 ? To : From; }

	// Divide

	template <typename T, typename dummy = void>
	static void CWDivide(T& InBase, const double Divider) { InBase /= Divider; }

	template <typename dummy = void>
	static void CWDivide(FRotator& InBase, const double Divider)
	{
		InBase.Yaw = InBase.Yaw / Divider;
		InBase.Pitch = InBase.Pitch / Divider;
		InBase.Roll = InBase.Roll / Divider;
	}

	template <typename T>
	static int SignPlus(const T& InValue)
	{
		int sign = FMath::Sign(InValue);
		return sign == 0 ? 1 : sign;
	}

	template <typename T>
	static int SignMinus(const T& InValue)
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

	////

	template <typename T, typename dummy = void>
	static T Add(const T& InBase, const T& Other) { return InBase + Other; }

	template <typename dummy = void>
	static FName Add(const FName& InBase, const FName& Other) { return InBase; }

	template <typename dummy = void>
	static FString Add(const FString& InBase, const FString& Other) { return InBase; }

	template <typename dummy = void>
	static bool Add(const bool& InBase, const bool& Other) { return Other ? true : InBase; }
}
