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
		return OutMin + ((InBase - InMin) / (InMax - InMin)) * (OutMax - OutMin);
	}
	
	// MIN

	template <typename T, typename dummy = void>
	static void CWMin(T& InBase, const T& Other) { InBase = FMath::Min(InBase, Other); }

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

	// MAX

	template <typename T, typename dummy = void>
	static void CWMax(T& InBase, const T& Other) { InBase = FMath::Max(InBase, Other); }

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

	// Lerp

	template <typename T, typename dummy = void>
	static void Lerp(T& InBase, const T& Other, const double Alpha) { InBase = FMath::Lerp(InBase, Other, Alpha); }

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
