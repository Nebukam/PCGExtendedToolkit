// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExMathContrast.generated.h"

/**
 * Contrast curve types for noise adjustment
 */
UENUM(BlueprintType)
enum class EPCGExContrastCurve : uint8
{
	Power UMETA(DisplayName = "Power", ToolTip = "Power curve - simple and predictable"),
	SCurve UMETA(DisplayName = "S-Curve (Sigmoid)", ToolTip = "Smooth S-curve using tanh - never clips"),
	Gain UMETA(DisplayName = "Gain", ToolTip = "Attempt function S-curve - symmetrical, subtle"),
};

namespace PCGExMath
{
	namespace Contrast
	{
		//
		// Contrast Functions
		// All functions expect input in [-1, 1] range and return values in [-1, 1]
		// Contrast parameter: 1.0 = no change, >1 = more contrast, <1 = less contrast
		//

		/**
		 * Power-based contrast (simple, predictable)
		 * Formula: sign(v) * |v|^(1/c)
		 * @param Value Input value in [-1, 1]
		 * @param Contrast Contrast amount (1.0 = no change, >1 = more contrast)
		 */
		FORCEINLINE double ContrastPower(const double Value, const double Contrast)
		{
			if (Contrast <= SMALL_NUMBER || FMath::Abs(Value) < SMALL_NUMBER) { return Value; }
			const double Exp = 1.0 / Contrast;
			return FMath::Sign(Value) * FMath::Pow(FMath::Abs(Value), Exp);
		}

		/**
		 * S-curve contrast using tanh (smooth, never clips)
		 * Formula: tanh(v * c) / tanh(c)
		 * @param Value Input value in [-1, 1]
		 * @param Contrast Contrast amount (1.0 = no change, >1 = more contrast)
		 */
		FORCEINLINE double ContrastSCurve(const double Value, const double Contrast)
		{
			if (Contrast <= SMALL_NUMBER) { return Value; }
			const double TanhC = FMath::Tanh(Contrast);
			if (FMath::Abs(TanhC) < SMALL_NUMBER) { return Value; }
			return FMath::Tanh(Value * Contrast) / TanhC;
		}

		/**
		 * Attempt contrast using sine-based gain function (symmetrical S-curve)
		 * Good for subtle adjustments, softer than sigmoid
		 * @param Value Input value in [-1, 1]
		 * @param Contrast Contrast amount (1.0 = no change, >1 = more contrast)
		 */
		FORCEINLINE double ContrastGain(const double Value, const double Contrast)
		{
			if (FMath::IsNearlyEqual(Contrast, 1.0, SMALL_NUMBER)) { return Value; }

			// Remap to [0,1] for gain calculation
			const double T = Value * 0.5 + 0.5;

			// Attempt function for S-curve: attempt = 0.5 * ((2*t)^(1/c)) for t < 0.5
			double Result;
			if (T < 0.5)
			{
				Result = 0.5 * FMath::Pow(2.0 * T, Contrast);
			}
			else
			{
				Result = 1.0 - 0.5 * FMath::Pow(2.0 * (1.0 - T), Contrast);
			}

			// Remap back to [-1,1]
			return Result * 2.0 - 1.0;
		}

		/**
		 * Apply contrast with selectable curve type
		 * @param Value Input value in [-1, 1]
		 * @param Contrast Contrast amount (1.0 = no change)
		 * @param CurveType 0 = Power, 1 = SCurve, 2 = Gain
		 */
		FORCEINLINE double ApplyContrast(const double Value, const double Contrast, const int32 CurveType = 0)
		{
			if (FMath::IsNearlyEqual(Contrast, 1.0, SMALL_NUMBER)) { return Value; }

			switch (CurveType)
			{
			case 0: return ContrastPower(Value, Contrast);
			case 1: return ContrastSCurve(Value, Contrast);
			case 2: return ContrastGain(Value, Contrast);
			default: return ContrastPower(Value, Contrast);
			}
		}

		//
		// Vector overloads
		//

		FORCEINLINE FVector2D ApplyContrast(const FVector2D& Value, const double Contrast, const int32 CurveType = 0)
		{
			if (FMath::IsNearlyEqual(Contrast, 1.0, SMALL_NUMBER)) { return Value; }
			return FVector2D(
				ApplyContrast(Value.X, Contrast, CurveType),
				ApplyContrast(Value.Y, Contrast, CurveType)
			);
		}

		FORCEINLINE FVector ApplyContrast(const FVector& Value, const double Contrast, const int32 CurveType = 0)
		{
			if (FMath::IsNearlyEqual(Contrast, 1.0, SMALL_NUMBER)) { return Value; }
			return FVector(
				ApplyContrast(Value.X, Contrast, CurveType),
				ApplyContrast(Value.Y, Contrast, CurveType),
				ApplyContrast(Value.Z, Contrast, CurveType)
			);
		}

		FORCEINLINE FVector4 ApplyContrast(const FVector4& Value, const double Contrast, const int32 CurveType = 0)
		{
			if (FMath::IsNearlyEqual(Contrast, 1.0, SMALL_NUMBER)) { return Value; }
			return FVector4(
				ApplyContrast(Value.X, Contrast, CurveType),
				ApplyContrast(Value.Y, Contrast, CurveType),
				ApplyContrast(Value.Z, Contrast, CurveType),
				ApplyContrast(Value.W, Contrast, CurveType)
			);
		}

		//
		// Batch operations (switch outside loop for branch prediction)
		//

		inline void ApplyContrastBatch(double* RESTRICT Values, const int32 Count, const double Contrast, const int32 CurveType = 0)
		{
			if (FMath::IsNearlyEqual(Contrast, 1.0, SMALL_NUMBER)) { return; }

			switch (CurveType)
			{
			case 0: // Power
				{
					const double Exp = 1.0 / Contrast;
					for (int32 i = 0; i < Count; ++i)
					{
						const double V = Values[i];
						if (FMath::Abs(V) > SMALL_NUMBER)
						{
							Values[i] = FMath::Sign(V) * FMath::Pow(FMath::Abs(V), Exp);
						}
					}
				}
				break;

			case 1: // SCurve
				{
					const double TanhC = FMath::Tanh(Contrast);
					const double InvTanhC = 1.0 / TanhC;
					for (int32 i = 0; i < Count; ++i)
					{
						Values[i] = FMath::Tanh(Values[i] * Contrast) * InvTanhC;
					}
				}
				break;

			case 2: // Gain
				for (int32 i = 0; i < Count; ++i)
				{
					Values[i] = ContrastGain(Values[i], Contrast);
				}
				break;

			default:
				ApplyContrastBatch(Values, Count, Contrast, 0);
				break;
			}
		}

		inline void ApplyContrastBatch(FVector2D* RESTRICT Values, const int32 Count, const double Contrast, const int32 CurveType = 0)
		{
			if (FMath::IsNearlyEqual(Contrast, 1.0, SMALL_NUMBER)) { return; }

			for (int32 i = 0; i < Count; ++i)
			{
				Values[i] = ApplyContrast(Values[i], Contrast, CurveType);
			}
		}

		inline void ApplyContrastBatch(FVector* RESTRICT Values, const int32 Count, const double Contrast, const int32 CurveType = 0)
		{
			if (FMath::IsNearlyEqual(Contrast, 1.0, SMALL_NUMBER)) { return; }

			for (int32 i = 0; i < Count; ++i)
			{
				Values[i] = ApplyContrast(Values[i], Contrast, CurveType);
			}
		}

		inline void ApplyContrastBatch(FVector4* RESTRICT Values, const int32 Count, const double Contrast, const int32 CurveType = 0)
		{
			if (FMath::IsNearlyEqual(Contrast, 1.0, SMALL_NUMBER)) { return; }

			for (int32 i = 0; i < Count; ++i)
			{
				Values[i] = ApplyContrast(Values[i], Contrast, CurveType);
			}
		}

		//
		// TArrayView convenience overloads
		//

		inline void ApplyContrastBatch(TArrayView<double> Values, const double Contrast, const int32 CurveType = 0)
		{
			ApplyContrastBatch(Values.GetData(), Values.Num(), Contrast, CurveType);
		}

		inline void ApplyContrastBatch(TArrayView<FVector2D> Values, const double Contrast, const int32 CurveType = 0)
		{
			ApplyContrastBatch(Values.GetData(), Values.Num(), Contrast, CurveType);
		}

		inline void ApplyContrastBatch(TArrayView<FVector> Values, const double Contrast, const int32 CurveType = 0)
		{
			ApplyContrastBatch(Values.GetData(), Values.Num(), Contrast, CurveType);
		}

		inline void ApplyContrastBatch(TArrayView<FVector4> Values, const double Contrast, const int32 CurveType = 0)
		{
			ApplyContrastBatch(Values.GetData(), Values.Num(), Contrast, CurveType);
		}
	}
}
