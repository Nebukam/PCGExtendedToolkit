// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"

namespace PCGExMath
{
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

		void Scale(double InScale)
		{
			TowardStart *= InScale;
			TowardEnd *= InScale;
		}

		void Extend(double InSize)
		{
			TowardStart += Direction * InSize;
			TowardEnd += Direction * -InSize;
		}

		static FApex FromStartOnly(const FVector& Start, const FVector& InApex) { return FApex(Start, InApex, InApex); }
		static FApex FromEndOnly(const FVector& End, const FVector& InApex) { return FApex(InApex, End, InApex); }
	};

	struct PCGEXTENDEDTOOLKIT_API FPathInfos
	{
		FPathInfos()
		{
		}

		FPathInfos(const FVector& InStart)
		{
			Start = InStart;
			Last = InStart;
			Length = 0;
		}

		FVector Start = FVector::ZeroVector;
		FVector Last = FVector::ZeroVector;
		double Length = -1;

		double Add(const FVector& Location)
		{
			Length += DistToLast(Location);
			Last = Location;
			return Length;
		}

		bool IsValid() const { return Length > 0; }
		double GetTime(const double Distance) const { return (!Distance || !Length) ? 0 : Distance / Length; }
		double DistToLast(const FVector& Location) const { return FVector::DistSquared(Last, Location); }
		bool IsLastWithinRange(const FVector& Location, const double Range) const { return DistToLast(Location) < Range; }
	};

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
