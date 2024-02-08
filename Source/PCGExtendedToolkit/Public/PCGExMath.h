// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "Elements/PCGDistance.h"

#include "PCGExMath.generated.h"

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

		void Scale(const double InScale)
		{
			TowardStart *= InScale;
			TowardEnd *= InScale;
		}

		void Extend(const double InSize)
		{
			TowardStart += Direction * InSize;
			TowardEnd += Direction * -InSize;
		}

		static FApex FromStartOnly(const FVector& Start, const FVector& InApex) { return FApex(Start, InApex, InApex); }
		static FApex FromEndOnly(const FVector& End, const FVector& InApex) { return FApex(InApex, End, InApex); }
	};

	struct PCGEXTENDEDTOOLKIT_API FPathMetrics
	{
		FPathMetrics()
		{
		}

		explicit FPathMetrics(const FVector& InStart)
		{
			Add(InStart);
		}

		explicit FPathMetrics(const TArrayView<FPCGPoint>& Points)
		{
			for (const FPCGPoint& Pt : Points) { Add(Pt.Transform.GetLocation()); }
		}

		FPathMetrics(const FPathMetrics& Other)
			: Start(Other.Start),
			  Last(Other.Last),
			  Length(Other.Length),
			  Count(Other.Count)
		{
		}

		FVector Start;
		FVector Last;
		double Length = -1;
		int32 Count = 0;

		void Reset(const FVector& InStart)
		{
			Start = InStart;
			Last = InStart;
			Length = 0;
			Count = 1;
		}

		double Add(const FVector& Location)
		{
			if (Length == -1)
			{
				Reset(Location);
				return 0;
			}
			Length += DistToLast(Location);
			Last = Location;
			Count++;
			return Length;
		}

		bool IsValid() const { return Length > 0; }
		double GetTime(const double Distance) const { return (!Distance || !Length) ? 0 : Distance / Length; }
		double DistToLast(const FVector& Location) const { return FVector::DistSquared(Last, Location); }
		bool IsLastWithinRange(const FVector& Location, const double Range) const { return DistToLast(Location) < Range; }
	};

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

	template <typename T, typename CompilerSafety = void>
	static T Tile(const T Value, const T InMin, const T InMax, const bool bSafe = false)
	{
		T Min = InMin;
		T Max = InMax;
		T OutValue = Value;
		T Range = Max - Min + 1;

		if (bSafe)
		{
			// Ensure the range is positive
			Min = FMath::Min(InMin, InMax);
			Max = FMath::Max(InMin, InMax);
			Range = Max - Min + 1;
		}

		// Wrap the value within the specified range
		OutValue = FMath::Fmod(static_cast<double>(OutValue - Min), static_cast<double>(Range));

		// Handle negative results
		if (OutValue < 0) { OutValue += Range; }

		// Shift back to the original range
		OutValue += Min;

		return OutValue;
	}

	template <typename CompilerSafety = void>
	static FVector2D Tile(const FVector2D Value, const FVector2D Min, const FVector2D Max, const bool bSafe = false)
	{
		return FVector2D(
			Tile(Value.X, Min.X, Max.X, bSafe),
			Tile(Value.Y, Min.Y, Max.Y, bSafe));
	}

	template <typename CompilerSafety = void>
	static FVector Tile(const FVector Value, const FVector Min, const FVector Max, const bool bSafe = false)
	{
		return FVector(
			Tile(Value.X, Min.X, Max.X, bSafe),
			Tile(Value.Y, Min.Y, Max.Y, bSafe),
			Tile(Value.Z, Min.Z, Max.Z, bSafe));
	}

	template <typename CompilerSafety = void>
	static FVector4 Tile(const FVector4 Value, const FVector4 Min, const FVector4 Max, const bool bSafe = false)
	{
		return FVector4(
			Tile(Value.X, Min.X, Max.X, bSafe),
			Tile(Value.Y, Min.Y, Max.Y, bSafe),
			Tile(Value.Z, Min.Z, Max.Z, bSafe),
			Tile(Value.W, Min.W, Max.W, bSafe));
	}

	template <typename T>
	static int SignPlus(const T& InValue)
	{
		const int Sign = FMath::Sign(InValue);
		return Sign == 0 ? 1 : Sign;
	}

	template <typename T>
	static int SignMinus(const T& InValue)
	{
		const int Sign = FMath::Sign(InValue);
		return Sign == 0 ? -1 : Sign;
	}

	static FBox ConeBox(const FVector& Center, const FVector& ConeDirection, const double Size)
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

	static double S_U(
		const FVector& A, const FVector& B, const FVector& C, const FVector& D,
		const FVector& E, const FVector& F, const FVector& G, const FVector& H)
	{
		return (A.Z - B.Z) * (C.X * D.Y - D.X * C.Y) - (E.Z - F.Z) * (G.X * H.Y - H.X * G.Y);
	};

	static double S_D(
		const int FirstComponent, const int SecondComponent,
		FVector A, FVector B, FVector C)
	{
		return
			A[FirstComponent] * (B[SecondComponent] - C[SecondComponent]) +
			B[FirstComponent] * (C[SecondComponent] - A[SecondComponent]) +
			C[FirstComponent] * (A[SecondComponent] - B[SecondComponent]);
	};

	static double S_E(
		const int FirstComponent, const int SecondComponent,
		const FVector& A, const FVector& B, const FVector& C, const FVector& D,
		const double RA, const double RB, const double RC, const double RD, const double UVW)
	{
		return (RA * S_D(FirstComponent, SecondComponent, B, C, D) - RB * S_D(FirstComponent, SecondComponent, C, D, A) +
			RC * S_D(FirstComponent, SecondComponent, D, A, B) - RD * S_D(FirstComponent, SecondComponent, A, B, C)) / UVW;
	};

	static double S_SQ(const FVector& P) { return P.X * P.X + P.Y * P.Y + P.Z * P.Z; };

	static bool FindSphereFrom4Points(const FVector& A, const FVector& B, const FVector& C, const FVector& D, FSphere& OutSphere)
	{
		//Shamelessly stolen from https://stackoverflow.com/questions/37449046/how-to-calculate-the-sphere-center-with-4-points

		const double U = S_U(A, B, C, D, B, C, D, A);
		const double V = S_U(C, D, A, B, D, A, B, C);
		const double W = S_U(A, C, D, B, B, D, A, C);
		const double UVW = 2 * (U + V + W);

		if (UVW == 0.0) { return false; } // Coplanar

		constexpr int C_X = 0;
		constexpr int C_Y = 1;
		constexpr int C_Z = 2;
		const double RA = S_SQ(A);
		const double RB = S_SQ(B);
		const double RC = S_SQ(C);
		const double RD = S_SQ(D);

		const FVector Center = FVector(
			S_E(C_Y, C_Z, A, B, C, D, RA, RB, RC, RD, UVW),
			S_E(C_Z, C_X, A, B, C, D, RA, RB, RC, RD, UVW),
			S_E(C_X, C_Y, A, B, C, D, RA, RB, RC, RD, UVW));

		const double radius = FMath::Sqrt(S_SQ(FVector(A - Center)));

		OutSphere = FSphere(Center, radius);
		return true;
	}

	template <typename T>
	static T GetMinMax(const TArray<T>& Values, T& OutMin, T& OutMax)
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
	static void SignedNormalize(TArray<T> Values)
	{
		T Min;
		T Max;
		GetMinMax(Values, Min, Max);
		T Range = FMath::Max(FMath::Abs(Max), FMath::Abs(Min));
		for (int i = 0; i < Values.Num(); i++) { Values[i] = Values[i] / Range; }
	}

	template <typename T>
	static void Remap(TArray<T> Values, bool bZeroMin = false, T Range = 1)
	{
		T Min;
		T Max;
		GetMinMax(Values, Min, Max);
		if (bZeroMin) { for (int i = 0; i < Values.Num(); i++) { Values[i] = Remap(Values[i], 0, Max, 0, 1) * Range; } }
		else { for (int i = 0; i < Values.Num(); i++) { Values[i] = Remap(Values[i], Min, Max, 0, 1) * Range; } }
	}

	template <typename T>
	static void Remap(TArray<T> Values, T Min, T Max, T Range = 1)
	{
		for (int i = 0; i < Values.Num(); i++) { Values[i] = Remap(Values[i], Min, Max, 0, 1) * Range; }
	}

	template <typename T>
	static T GetAverage(const TArray<T>& Values)
	{
		T Sum = 0;
		for (const T Value : Values) { Sum += Value; }
		return Div(Sum, Values.Num());
	}

	template <typename T>
	static T GetMedian(const TArray<T>& Values)
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

	static double GetMode(const TArray<double>& Values, bool bHighest, uint32 Tolerance = 5)
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

#pragma region Add

	template <typename T, typename CompilerSafety = void>
	static T Add(const T& A, const T& B, const double& Alpha = 0) { return A + B; }

	template <typename CompilerSafety = void>
	static bool Add(const bool& A, const bool& B, const double& Alpha = 0) { return B ? true : A; }

	template <typename CompilerSafety = void>
	static FTransform Add(const FTransform& A, const FTransform& B, const double& Alpha = 0)
	{
		return FTransform(
			A.GetRotation() + B.GetRotation(),
			A.GetLocation() + B.GetLocation(),
			A.GetScale3D() + B.GetScale3D());
	}

	template <typename CompilerSafety = void>
	static FString Add(const FString& A, const FString& B, const double& Alpha = 0) { return A < B ? B : A; }

	template <typename CompilerSafety = void>
	static FName Add(const FName& A, const FName& B, const double& Alpha = 0) { return A.ToString() < B.ToString() ? B : A; }

#pragma endregion

#pragma region Sub

	template <typename T, typename CompilerSafety = void>
	static T Sub(const T& A, const T& B, const double& Alpha = 0) { return A - B; }

	template <typename CompilerSafety = void>
	static bool Sub(const bool& A, const bool& B, const double& Alpha = 0) { return B ? true : A; }

	template <typename CompilerSafety = void>
	static FTransform Sub(const FTransform& A, const FTransform& B, const double& Alpha = 0)
	{
		return FTransform(
			A.GetRotation() - B.GetRotation(),
			A.GetLocation() - B.GetLocation(),
			A.GetScale3D() - B.GetScale3D());
	}

	template <typename CompilerSafety = void>
	static FString Sub(const FString& A, const FString& B, const double& Alpha = 0) { return A < B ? A : B; }

	template <typename CompilerSafety = void>
	static FName Sub(const FName& A, const FName& B, const double& Alpha = 0) { return A.ToString() < B.ToString() ? A : B; }

#pragma endregion

#pragma region Min

	template <typename T, typename CompilerSafety = void>
	static T Min(const T& A, const T& B, const double& Alpha = 0) { return FMath::Min(A, B); }

	template <typename CompilerSafety = void>
	static FVector2D Min(const FVector2D& A, const FVector2D& B, const double& Alpha = 0)
	{
		return FVector2D(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y));
	}

	template <typename CompilerSafety = void>
	static FVector Min(const FVector& A, const FVector& B, const double& Alpha = 0)
	{
		return FVector(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y),
			FMath::Min(A.Z, B.Z));
	}

	template <typename CompilerSafety = void>
	static FVector4 Min(const FVector4& A, const FVector4& B, const double& Alpha = 0)
	{
		return FVector4(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y),
			FMath::Min(A.Z, B.Z),
			FMath::Min(A.W, B.W));
	}

	template <typename CompilerSafety = void>
	static FRotator Min(const FRotator& A, const FRotator& B, const double& Alpha = 0)
	{
		return FRotator(
			FMath::Min(A.Pitch, B.Pitch),
			FMath::Min(A.Yaw, B.Yaw),
			FMath::Min(A.Roll, B.Roll));
	}

	template <typename CompilerSafety = void>
	static FQuat Min(const FQuat& A, const FQuat& B, const double& Alpha = 0) { return Min(A.Rotator(), B.Rotator()).Quaternion(); }

	template <typename CompilerSafety = void>
	static FTransform Min(const FTransform& A, const FTransform& B, const double& Alpha = 0)
	{
		return FTransform(
			Min(A.GetRotation(), B.GetRotation()),
			Min(A.GetLocation(), B.GetLocation()),
			Min(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename CompilerSafety = void>
	static FString Min(const FString& A, const FString& B, const double& Alpha = 0) { return A > B ? B : A; }

	template <typename CompilerSafety = void>
	static FName Min(const FName& A, const FName& B, const double& Alpha = 0) { return A.ToString() > B.ToString() ? B : A; }

#pragma endregion

#pragma region Max

	template <typename T, typename CompilerSafety = void>
	static T Max(const T& A, const T& B, const double& Alpha = 0) { return FMath::Max(A, B); }

	template <typename CompilerSafety = void>
	static FVector2D Max(const FVector2D& A, const FVector2D& B, const double& Alpha = 0)
	{
		return FVector2D(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y));
	}

	template <typename CompilerSafety = void>
	static FVector Max(const FVector& A, const FVector& B, const double& Alpha = 0)
	{
		return FVector(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y),
			FMath::Max(A.Z, B.Z));
	}

	template <typename CompilerSafety = void>
	static FVector4 Max(const FVector4& A, const FVector4& B, const double& Alpha = 0)
	{
		return FVector4(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y),
			FMath::Max(A.Z, B.Z),
			FMath::Max(A.W, B.W));
	}

	template <typename CompilerSafety = void>
	static FRotator Max(const FRotator& A, const FRotator& B, const double& Alpha = 0)
	{
		return FRotator(
			FMath::Max(A.Pitch, B.Pitch),
			FMath::Max(A.Yaw, B.Yaw),
			FMath::Max(A.Roll, B.Roll));
	}

	template <typename CompilerSafety = void>
	static FQuat Max(const FQuat& A, const FQuat& B, const double& Alpha = 0) { return Max(A.Rotator(), B.Rotator()).Quaternion(); }

	template <typename CompilerSafety = void>
	static FTransform Max(const FTransform& A, const FTransform& B, const double& Alpha = 0)
	{
		return FTransform(
			Max(A.GetRotation(), B.GetRotation()),
			Max(A.GetLocation(), B.GetLocation()),
			Max(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename CompilerSafety = void>
	static FString Max(const FString& A, const FString& B, const double& Alpha = 0) { return A > B ? A : B; }

	template <typename CompilerSafety = void>
	static FName Max(const FName& A, const FName& B, const double& Alpha = 0) { return A.ToString() > B.ToString() ? A : B; }

#pragma endregion

#pragma region Lerp

	template <typename T, typename CompilerSafety = void>
	static T Lerp(const T& A, const T& B, const double& Alpha = 0) { return FMath::Lerp(A, B, Alpha); }

	template <typename CompilerSafety = void>
	static FColor Lerp(const FColor& A, const FColor& B, const double& Alpha = 0) { return FMath::Lerp(A.ReinterpretAsLinear(), B.ReinterpretAsLinear(), Alpha).ToFColor(false); }

	template <typename CompilerSafety = void>
	static FQuat Lerp(const FQuat& A, const FQuat& B, const double& Alpha = 0) { return FQuat::Slerp(A, B, Alpha); }

	template <typename CompilerSafety = void>
	static FTransform Lerp(const FTransform& A, const FTransform& B, const double& Alpha = 0)
	{
		return FTransform(
			Lerp(A.GetRotation(), B.GetRotation(), Alpha),
			Lerp(A.GetLocation(), B.GetLocation(), Alpha),
			Lerp(A.GetScale3D(), B.GetScale3D(), Alpha));
	}

	template <typename CompilerSafety = void>
	static FString Lerp(const FString& A, const FString& B, const double& Alpha = 0) { return Alpha > 0.5 ? B : A; }

	template <typename CompilerSafety = void>
	static FName Lerp(const FName& A, const FName& B, const double& Alpha = 0) { return Alpha > 0.5 ? B : A; }

#pragma endregion

#pragma region Divide

	template <typename T, typename CompilerSafety = void>
	static T Div(const T& A, const double Divider) { return A / Divider; }

	template <typename CompilerSafety = void>
	static bool Div(const bool& A, const double Divider) { return A; }

	template <typename CompilerSafety = void>
	static FRotator Div(const FRotator& A, const double Divider)
	{
		return FRotator(
			A.Pitch / Divider,
			A.Yaw / Divider,
			A.Roll / Divider);
	}

	template <typename CompilerSafety = void>
	static FQuat Div(const FQuat& A, const double Divider) { return Div(A.Rotator(), Divider).Quaternion(); }


	template <typename CompilerSafety = void>
	static FTransform Div(const FTransform& A, const double Divider)
	{
		return FTransform(
			Div(A.GetRotation(), Divider),
			A.GetLocation() / Divider,
			A.GetScale3D() / Divider);
	}

	template <typename CompilerSafety = void>
	static FString Div(const FString& A, const double Divider) { return A; }

	template <typename CompilerSafety = void>
	static FName Div(const FName& A, const double Divider) { return A; }

#pragma endregion

#pragma region Copy

	template <typename T>
	static T Copy(const T& A, const T& B, const double& Alpha = 0) { return B; }

#pragma endregion

#pragma region NoBlend

	template <typename T>
	static T NoBlend(const T& A, const T& B, const double& Alpha = 0) { return A; }

#pragma endregion

#pragma region Component

	template <typename T, typename CompilerSafety = void>
	static double GetComponent(const T& A, const int32 Index) { return A; }

	template <typename CompilerSafety = void>
	static double GetComponent(const bool& A, const int32 Index) { return A ? 1 : 0; }

	template <typename CompilerSafety = void>
	static double GetComponent(const FVector& A, const int32 Index) { return A[Index]; }

	template <typename CompilerSafety = void>
	static double GetComponent(const FVector2D& A, const int32 Index) { return A[Index]; }

	template <typename CompilerSafety = void>
	static double GetComponent(const FVector4& A, const int32 Index) { return A[Index]; }

	template <typename CompilerSafety = void>
	static double GetComponent(const FRotator& A, const int32 Index) { return A.Euler()[Index]; }

	template <typename CompilerSafety = void>
	static double GetComponent(const FQuat& A, const int32 Index) { return A.Euler()[Index]; }

	template <typename CompilerSafety = void>
	static double GetComponent(const FTransform& A, const int32 Index) { return A.GetLocation()[Index]; }

	template <typename CompilerSafety = void>
	static double GetComponent(const FName& A, const int32 Index) { return -1; }

	template <typename CompilerSafety = void>
	static double GetComponent(const FString& A, const int32 Index) { return -1; }

	////

	template <typename T, typename CompilerSafety = void>
	static void SetComponent(T& A, const int32 Index, const double InValue) { A = InValue; }

	template <typename CompilerSafety = void>
	static void SetComponent(bool& A, const int32 Index, const double InValue) { A = InValue <= 0 ? false : true; }

	template <typename CompilerSafety = void>
	static void SetComponent(FVector& A, const int32 Index, const double InValue) { A[Index] = InValue; }

	template <typename CompilerSafety = void>
	static void SetComponent(FVector2D& A, const int32 Index, const double InValue) { A[Index] = InValue; }

	template <typename CompilerSafety = void>
	static void SetComponent(FVector4& A, const int32 Index, const double InValue) { A[Index] = InValue; }

	template <typename CompilerSafety = void>
	static void SetComponent(FRotator& A, const int32 Index, const double InValue)
	{
		FVector Euler = A.Euler();
		SetComponent(Euler, Index, InValue);
		A = FRotator::MakeFromEuler(Euler);
	}

	template <typename CompilerSafety = void>
	static void SetComponent(FQuat& A, const int32 Index, const double InValue)
	{
		FVector Euler = A.Euler();
		SetComponent(Euler, Index, InValue);
		A = FQuat::MakeFromEuler(Euler);
	}

	template <typename CompilerSafety = void>
	static void SetComponent(FTransform& A, const int32 Index, const double InValue)
	{
		FVector Location = A.GetLocation();
		SetComponent(Location, Index, InValue);
		A.SetLocation(Location);
	}

	template <typename CompilerSafety = void>
	static void SetComponent(FName& A, const int32 Index, const double InValue)
	{
	}

	template <typename CompilerSafety = void>
	static void SetComponent(FString& A, const int32 Index, const double InValue)
	{
	}

#pragma endregion

	template <typename T>
	static T SanitizeIndex(const T& Index, const T& Limit, const EPCGExIndexSafety Method)
	{
		switch (Method)
		{
		case EPCGExIndexSafety::Ignore:
			if (Index < 0 || Index > Limit) { return -1; }
			break;
		case EPCGExIndexSafety::Tile:
			return PCGExMath::Tile(Index, 0, Limit);
		case EPCGExIndexSafety::Clamp:
			return FMath::Clamp(Index, 0, Limit);
		default: ;
		}
		return Index;
	}

	static FVector GetDirection(const FQuat& Quat, const EPCGExAxis Dir)
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
		case EPCGExAxis::Euler:
			return Quat.Euler() * -1;
		}
	}

	static FVector GetDirection(const EPCGExAxis Dir)
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
		case EPCGExAxis::Euler:
			return FVector::OneVector;
		}
	}

	static FQuat MakeDirection(const EPCGExAxis Dir, const FVector& InForward)
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

	static FQuat MakeDirection(const EPCGExAxis Dir, const FVector& InForward, const FVector& InUp)
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
	static void Swap(TArray<T>& Array, int32 FirstIndex, int32 SecondIndex)
	{
		T* Ptr1 = &Array[FirstIndex];
		T* Ptr2 = &Array[SecondIndex];
		std::swap(*Ptr1, *Ptr2);
	}

	static void RandomizeSeed(FPCGPoint& Point, const FVector& Offset = FVector::ZeroVector)
	{
		Point.Seed = static_cast<int32>(Remap(
			FMath::PerlinNoise3D(Tile(Point.Transform.GetLocation() * 0.001 + Offset, FVector(-1), FVector(1))),
			-1, 1, TNumericLimits<int32>::Min(), TNumericLimits<int32>::Max()));
	}

	// Stolen from PCGDistance
	static FVector GetRelationalCenter(
		const EPCGExDistance Shape,
		const FPCGPoint& SourcePoint,
		const FVector& SourceCenter,
		const FVector& TargetCenter)
	{
		if (Shape == EPCGExDistance::SphereBounds)
		{
			FVector Dir = TargetCenter - SourceCenter;
			Dir.Normalize();

			return SourceCenter + Dir * SourcePoint.GetScaledExtents().Length();
		}
		if (Shape == EPCGExDistance::BoxBounds)
		{
			const FVector LocalTargetCenter = SourcePoint.Transform.InverseTransformPosition(TargetCenter);

			const double DistanceSquared = ComputeSquaredDistanceFromBoxToPoint(SourcePoint.BoundsMin, SourcePoint.BoundsMax, LocalTargetCenter);

			FVector Dir = -LocalTargetCenter;
			Dir.Normalize();

			const FVector LocalClosestPoint = LocalTargetCenter + Dir * FMath::Sqrt(DistanceSquared);

			return SourcePoint.Transform.TransformPosition(LocalClosestPoint);
		}

		// EPCGExDistance::Center
		return SourceCenter;
	}
	
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExClampSettings
{
	GENERATED_BODY()

	FPCGExClampSettings()
	{
	}

	FPCGExClampSettings(const FPCGExClampSettings& Other):
		bClampMin(Other.bClampMin),
		ClampMin(Other.ClampMin),
		bClampMax(Other.bClampMax),
		ClampMax(Other.ClampMax)
	{
	}

	/** Clamp minimum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bClampMin = false;

	/** Clamp minimum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bClampMin"))
	double ClampMin = 0;

	/** Clamp maximum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bClampMax = false;

	/** Clamp maximum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bClampMax"))
	double ClampMax = 0;

	double GetClampMin(const double InValue) const { return InValue < ClampMin ? ClampMin : InValue; }
	double GetClampMax(const double InValue) const { return InValue > ClampMax ? ClampMax : InValue; }
	double GetClampMinMax(const double InValue) const { return InValue > ClampMax ? ClampMax : InValue < ClampMin ? ClampMin : InValue; }

	double GetClampedValue(const double InValue) const
	{
		if (bClampMin && InValue < ClampMin) { return ClampMin; }
		if (bClampMax && InValue > ClampMax) { return ClampMax; }
		return InValue;
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExRemapSettings
{
	GENERATED_BODY()

	FPCGExRemapSettings()
	{
	}

	FPCGExRemapSettings(const FPCGExRemapSettings& Other):
		bInMin(Other.bInMin),
		InMin(Other.InMin),
		bInMax(Other.bInMax),
		InMax(Other.InMax),
		RangeMethod(Other.RangeMethod),
		Scale(Other.Scale),
		RemapCurveObj(Other.RemapCurveObj)
	{
	}

	~FPCGExRemapSettings()
	{
		RemapCurveObj = nullptr;
	}

	/** Fixed In Min value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bInMin = false;

	/** Fixed In Min value. If disabled, will use the lowest input value.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bInMin"))
	double InMin = 0;

	/** Fixed In Max value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bInMax = false;

	/** Fixed In Max value. If disabled, will use the highest input value.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bInMax"))
	double InMax = 0;

	/** How to remap before sampling the curve. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExRangeType RangeMethod = EPCGExRangeType::EffectiveRange;

	/** Scale output value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Scale = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UCurveFloat> RemapCurve = TSoftObjectPtr<UCurveFloat>(PCGEx::WeightDistributionLinear);

	TObjectPtr<UCurveFloat> RemapCurveObj = nullptr;

	void LoadCurve()
	{
		if (RemapCurve.IsNull()) { RemapCurveObj = TSoftObjectPtr<UCurveFloat>(PCGEx::WeightDistributionLinear).LoadSynchronous(); }
		else { RemapCurveObj = RemapCurve.LoadSynchronous(); }
	}

	double GetRemappedValue(double Value) const
	{
		return RemapCurveObj->GetFloatValue(PCGExMath::Remap(Value, InMin, InMax, 0, 1)) * Scale;
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExFuseSettings
{
	GENERATED_BODY()

	FPCGExFuseSettings()
	{
		Init();
	}

	FPCGExFuseSettings(double InTolerance)
		: Tolerance(InTolerance)
	{
		Init();
	}

	FPCGExFuseSettings(double InTolerance, EPCGExDistance SourceMethod)
		: Tolerance(InTolerance), SourceDistance(SourceMethod)
	{
		Init();
	}

	/** Fusing distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0.0001))
	double Tolerance = 0.001;
	double ToleranceSquared = 10;

	/** Uses a per-axis radius, manathan-style */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bComponentWiseTolerance = false;

	/** Component-wise radiuses */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bComponentWiseTolerance", EditConditionHides, ClampMin=0.0001))
	FVector Tolerances = FVector(0.001);

	/** Method used to compute the distance from the source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistance SourceDistance = EPCGExDistance::Center;

	void Init()
	{
		ToleranceSquared = Tolerance * Tolerance;
		Tolerances = FVector(Tolerance);
	}

	bool IsWithinTolerance(const double DistSquared) const
	{
		return FMath::IsWithin<double, double>(DistSquared, 0, ToleranceSquared);
	}

	bool IsWithinTolerance(const FVector& Source, const FVector& Target) const
	{
		return FMath::IsWithin<double, double>(FVector::DistSquared(Source, Target), 0, ToleranceSquared);
	}

	bool IsWithinToleranceComponentWise(const FVector& Source, const FVector& Target) const
	{
		return (FMath::IsWithin<double, double>(abs(Source.X - Target.X), 0, Tolerance) &&
			FMath::IsWithin<double, double>(abs(Source.Y - Target.Y), 0, Tolerance) &&
			FMath::IsWithin<double, double>(abs(Source.Z - Target.Z), 0, Tolerance));
	}

	///
	FVector GetSourceCenter(const FPCGPoint& SourcePoint, const FVector& SourceCenter, const FVector& TargetCenter) const
	{
		return PCGExMath::GetRelationalCenter(SourceDistance, SourcePoint, SourceCenter, TargetCenter);
	}

	double GetSourceDistSquared(const FPCGPoint& SourcePoint, const FVector& SourceCenter, const FVector& TargetCenter) const
	{
		return FVector::DistSquared(PCGExMath::GetRelationalCenter(SourceDistance, SourcePoint, SourceCenter, TargetCenter), TargetCenter);
	}

	bool IsWithinTolerance(const FPCGPoint& SourcePoint, const FVector& SourceCenter, const FVector& TargetCenter) const
	{
		return IsWithinTolerance(GetSourceCenter(SourcePoint, SourceCenter, TargetCenter), TargetCenter);
	}

	bool IsWithinToleranceComponentWise(const FPCGPoint& SourcePoint, const FVector& SourceCenter, const FVector& TargetCenter) const
	{
		return IsWithinToleranceComponentWise(GetSourceCenter(SourcePoint, SourceCenter, TargetCenter), TargetCenter);
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExFuseSettingsWithTarget : public FPCGExFuseSettings
{
	GENERATED_BODY()

	FPCGExFuseSettingsWithTarget()
	{
	}

	FPCGExFuseSettingsWithTarget(double InTolerance)
		: FPCGExFuseSettings(InTolerance)
	{
	}

	FPCGExFuseSettingsWithTarget(double InTolerance, EPCGExDistance SourceMethod)
		: FPCGExFuseSettings(InTolerance, SourceMethod)
	{
	}

	FPCGExFuseSettingsWithTarget(double InTolerance, EPCGExDistance SourceMethod, EPCGExDistance TargetMethod)
		: FPCGExFuseSettings(InTolerance, SourceMethod), TargetDistance(TargetMethod)
	{
	}

	/** Method used to compute the distance to the target */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistance TargetDistance = EPCGExDistance::Center;

	void GetCenters(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const
	{
		OutSource = GetSourceCenter(SourcePoint, SourcePoint.Transform.GetLocation(), TargetPoint.Transform.GetLocation());
		OutTarget = PCGExMath::GetRelationalCenter(TargetDistance, TargetPoint, TargetPoint.Transform.GetLocation(), OutSource);
	}

	bool IsWithinTolerance(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint) const
	{
		FVector A;
		FVector B;
		GetCenters(SourcePoint, TargetPoint, A, B);
		return FPCGExFuseSettings::IsWithinTolerance(A, B);
	}

	bool IsWithinToleranceComponentWise(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint) const
	{
		FVector A;
		FVector B;
		GetCenters(SourcePoint, TargetPoint, A, B);
		return FPCGExFuseSettings::IsWithinToleranceComponentWise(A, B);
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExDistanceSettings
{
	GENERATED_BODY()

	FPCGExDistanceSettings()
	{
	}

	FPCGExDistanceSettings(EPCGExDistance SourceMethod)
		: SourceDistance(SourceMethod)
	{
	}

	/** Method used to compute the distance from the source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistance SourceDistance = EPCGExDistance::Center;

	FVector GetSourceCenter(const FPCGPoint& SourcePoint, const FVector& SourceCenter, const FVector& TargetCenter) const
	{
		return PCGExMath::GetRelationalCenter(SourceDistance, SourcePoint, SourceCenter, TargetCenter);
	}

	double GetSourceDistanceToPosition(const FPCGPoint& SourcePoint, const FVector& SourceCenter, const FVector& TargetCenter) const
	{
		return FVector::DistSquared(GetSourceCenter(SourcePoint, SourceCenter, TargetCenter), TargetCenter);
	}
	
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExDistanceSettingsWithTarget : public FPCGExDistanceSettings
{
	GENERATED_BODY()

	FPCGExDistanceSettingsWithTarget()
	{
	}

	FPCGExDistanceSettingsWithTarget(EPCGExDistance SourceMethod)
		: FPCGExDistanceSettings(SourceMethod)
	{
	}

	FPCGExDistanceSettingsWithTarget(EPCGExDistance SourceMethod, EPCGExDistance TargetMethod)
		: FPCGExDistanceSettings(SourceMethod), TargetDistance(TargetMethod)
	{
	}

	/** Method used to compute the distance to the target */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDistance TargetDistance = EPCGExDistance::Center;

	FVector GetTargetCenter(const FPCGPoint& TargetPoint, const FVector& TargetCenter, const FVector& OtherCenter) const
	{
		return PCGExMath::GetRelationalCenter(TargetDistance, TargetPoint, TargetCenter, OtherCenter);
	}

	void GetCenters(const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const
	{
		const FVector TargetLocation = TargetPoint.Transform.GetLocation();
		OutSource = GetSourceCenter(SourcePoint, SourcePoint.Transform.GetLocation(), TargetLocation);
		OutTarget = GetTargetCenter(TargetPoint, TargetLocation, OutSource);
	}
};
