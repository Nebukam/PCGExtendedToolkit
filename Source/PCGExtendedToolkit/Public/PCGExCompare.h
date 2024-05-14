// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExCompare.generated.h"

#define PCGEX_UNSUPPORTED_STRING_TYPES(MACRO)\
MACRO(FString)\
MACRO(FName)\
MACRO(FSoftObjectPath)\
MACRO(FSoftClassPath)

#define PCGEX_UNSUPPORTED_PATH_TYPES(MACRO)\
MACRO(FSoftObjectPath)\
MACRO(FSoftClassPath)

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Comparison"))
enum class EPCGExComparison : uint8
{
	StrictlyEqual UMETA(DisplayName = " == ", Tooltip="Operand A Strictly Equal to Operand B"),
	StrictlyNotEqual UMETA(DisplayName = " != ", Tooltip="Operand A Strictly Not Equal to Operand B"),
	EqualOrGreater UMETA(DisplayName = " >= ", Tooltip="Operand A Equal or Greater to Operand B"),
	EqualOrSmaller UMETA(DisplayName = " <= ", Tooltip="Operand A Equal or Smaller to Operand B"),
	StrictlyGreater UMETA(DisplayName = " > ", Tooltip="Operand A Strictly Greater to Operand B"),
	StrictlySmaller UMETA(DisplayName = " < ", Tooltip="Operand A Strictly Smaller to Operand B"),
	NearlyEqual UMETA(DisplayName = " ~= ", Tooltip="Operand A Nearly Equal to Operand B"),
	NearlyNotEqual UMETA(DisplayName = " !~= ", Tooltip="Operand A Nearly Not Equal to Operand B"),
};

namespace PCGExCompare
{
	static FString ToString(const EPCGExComparison Comparison)
	{
		switch (Comparison)
		{
		case EPCGExComparison::StrictlyEqual:
			return " == ";
		case EPCGExComparison::StrictlyNotEqual:
			return " != ";
		case EPCGExComparison::EqualOrGreater:
			return " >= ";
		case EPCGExComparison::EqualOrSmaller:
			return " <= ";
		case EPCGExComparison::StrictlyGreater:
			return " > ";
		case EPCGExComparison::StrictlySmaller:
			return " < ";
		case EPCGExComparison::NearlyEqual:
			return " ~= ";
		case EPCGExComparison::NearlyNotEqual:
			return " !~= ";
		default: return " ?? ";
		}
	}


#pragma region StrictlyEqual

	template <typename T, typename CompilerSafety = void>
	static bool StrictlyEqual(const T& A, const T& B) { return A == B; }

#pragma endregion

#pragma region StrictlyNotEqual

	template <typename T, typename CompilerSafety = void>
	static bool StrictlyNotEqual(const T& A, const T& B) { return A != B; }

#pragma endregion

#pragma region EqualOrGreater

	template <typename T, typename CompilerSafety = void>
	static bool EqualOrGreater(const T& A, const T& B) { return A >= B; }

	template <typename CompilerSafety = void>
	static bool EqualOrGreater(const FVector2D& A, const FVector2D& B) { return A.SquaredLength() >= B.SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool EqualOrGreater(const FVector& A, const FVector& B) { return A.SquaredLength() >= B.SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool EqualOrGreater(const FVector4& A, const FVector4& B) { return FVector(A).SquaredLength() >= FVector(B).SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool EqualOrGreater(const FRotator& A, const FRotator& B) { return A.Euler().SquaredLength() >= B.Euler().SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool EqualOrGreater(const FQuat& A, const FQuat& B) { return A.Euler().SquaredLength() >= B.Euler().SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool EqualOrGreater(const FTransform& A, const FTransform& B)
	{
		return (
			EqualOrGreater(A.GetLocation(), B.GetLocation()) &&
			EqualOrGreater(A.GetRotation(), B.GetRotation()) &&
			EqualOrGreater(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename CompilerSafety = void>
	static bool EqualOrGreater(const FString& A, const FString& B) { return A.Len() >= B.Len(); }

	template <typename CompilerSafety = void>
	static bool EqualOrGreater(const FName& A, const FName& B) { return EqualOrGreater(A.ToString(), B.ToString()); }

#define PCGEX_UNSUPPORTED_EOG(_TYPE) template <typename CompilerSafety = void> static bool EqualOrGreater(const _TYPE& A, const _TYPE& B) { return false; }
	PCGEX_UNSUPPORTED_PATH_TYPES(PCGEX_UNSUPPORTED_EOG)
#undef PCGEX_UNSUPPORTED_EOG

#pragma endregion

#pragma region EqualOrSmaller

	template <typename T, typename CompilerSafety = void>
	static bool EqualOrSmaller(const T& A, const T& B) { return A <= B; }

	template <typename CompilerSafety = void>
	static bool EqualOrSmaller(const FVector2D& A, const FVector2D& B) { return A.SquaredLength() <= B.SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool EqualOrSmaller(const FVector& A, const FVector& B) { return A.SquaredLength() <= B.SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool EqualOrSmaller(const FVector4& A, const FVector4& B) { return FVector(A).SquaredLength() <= FVector(B).SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool EqualOrSmaller(const FRotator& A, const FRotator& B) { return A.Euler().SquaredLength() <= B.Euler().SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool EqualOrSmaller(const FQuat& A, const FQuat& B) { return A.Euler().SquaredLength() <= B.Euler().SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool EqualOrSmaller(const FTransform& A, const FTransform& B)
	{
		return (
			EqualOrSmaller(A.GetLocation(), B.GetLocation()) &&
			EqualOrSmaller(A.GetRotation(), B.GetRotation()) &&
			EqualOrSmaller(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename CompilerSafety = void>
	static bool EqualOrSmaller(const FString& A, const FString& B) { return A.Len() <= B.Len(); }

	template <typename CompilerSafety = void>
	static bool EqualOrSmaller(const FName& A, const FName& B) { return EqualOrSmaller(A.ToString(), B.ToString()); }

#define PCGEX_UNSUPPORTED_EOS(_TYPE) template <typename CompilerSafety = void> static bool EqualOrSmaller(const _TYPE& A, const _TYPE& B) { return false; }
	PCGEX_UNSUPPORTED_PATH_TYPES(PCGEX_UNSUPPORTED_EOS)
#undef PCGEX_UNSUPPORTED_EOS

#pragma endregion

#pragma region EqualOrSmaller

	template <typename T, typename CompilerSafety = void>
	static bool StrictlyGreater(const T& A, const T& B) { return A > B; }

	template <typename CompilerSafety = void>
	static bool StrictlyGreater(const FVector2D& A, const FVector2D& B) { return A.SquaredLength() > B.SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool StrictlyGreater(const FVector& A, const FVector& B) { return A.SquaredLength() > B.SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool StrictlyGreater(const FVector4& A, const FVector4& B) { return FVector(A).SquaredLength() > FVector(B).SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool StrictlyGreater(const FRotator& A, const FRotator& B) { return A.Euler().SquaredLength() > B.Euler().SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool StrictlyGreater(const FQuat& A, const FQuat& B) { return A.Euler().SquaredLength() > B.Euler().SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool StrictlyGreater(const FTransform& A, const FTransform& B)
	{
		return (
			StrictlyGreater(A.GetLocation(), B.GetLocation()) &&
			StrictlyGreater(A.GetRotation(), B.GetRotation()) &&
			StrictlyGreater(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename CompilerSafety = void>
	static bool StrictlyGreater(const FString& A, const FString& B) { return A.Len() > B.Len(); }

	template <typename CompilerSafety = void>
	static bool StrictlyGreater(const FName& A, const FName& B) { return StrictlyGreater(A.ToString(), B.ToString()); }

#define PCGEX_UNSUPPORTED_SG(_TYPE) template <typename CompilerSafety = void> static bool StrictlyGreater(const _TYPE& A, const _TYPE& B) { return false; }
	PCGEX_UNSUPPORTED_PATH_TYPES(PCGEX_UNSUPPORTED_SG)
#undef PCGEX_UNSUPPORTED_SG

#pragma endregion

#pragma region StrictlySmaller

	template <typename T, typename CompilerSafety = void>
	static bool StrictlySmaller(const T& A, const T& B) { return A < B; }

	template <typename CompilerSafety = void>
	static bool StrictlySmaller(const FVector2D& A, const FVector2D& B) { return A.SquaredLength() < B.SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool StrictlySmaller(const FVector& A, const FVector& B) { return A.SquaredLength() < B.SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool StrictlySmaller(const FVector4& A, const FVector4& B) { return FVector(A).SquaredLength() < FVector(B).SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool StrictlySmaller(const FRotator& A, const FRotator& B) { return A.Euler().SquaredLength() < B.Euler().SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool StrictlySmaller(const FQuat& A, const FQuat& B) { return A.Euler().SquaredLength() < B.Euler().SquaredLength(); }

	template <typename CompilerSafety = void>
	static bool StrictlySmaller(const FTransform& A, const FTransform& B)
	{
		return (
			StrictlySmaller(A.GetLocation(), B.GetLocation()) &&
			StrictlySmaller(A.GetRotation(), B.GetRotation()) &&
			StrictlySmaller(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename CompilerSafety = void>
	static bool StrictlySmaller(const FString& A, const FString& B) { return A.Len() < B.Len(); }

	template <typename CompilerSafety = void>
	static bool StrictlySmaller(const FName& A, const FName& B) { return EqualOrSmaller(A.ToString(), B.ToString()); }

#define PCGEX_UNSUPPORTED_SS(_TYPE) template <typename CompilerSafety = void> static bool StrictlySmaller(const _TYPE& A, const _TYPE& B) { return false; }
	PCGEX_UNSUPPORTED_PATH_TYPES(PCGEX_UNSUPPORTED_SS)
#undef PCGEX_UNSUPPORTED_SS

#pragma endregion

#pragma region NearlyEqual

	template <typename T, typename CompilerSafety = void>
	static bool NearlyEqual(const T& A, const T& B, const double Tolerance = 0.001) { return FMath::IsNearlyEqual(A, B, Tolerance); }

	template <typename CompilerSafety = void>
	static bool NearlyEqual(const FVector2D& A, const FVector2D& B, const double Tolerance)
	{
		return (
			NearlyEqual(A.X, B.X, Tolerance) &&
			NearlyEqual(A.Y, B.Y, Tolerance));
	}

	template <typename CompilerSafety = void>
	static bool NearlyEqual(const FVector& A, const FVector& B, const double Tolerance)
	{
		return (
			NearlyEqual(A.X, B.X, Tolerance) &&
			NearlyEqual(A.Y, B.Y, Tolerance) &&
			NearlyEqual(A.Z, B.Z, Tolerance));
	}

	template <typename CompilerSafety = void>
	static bool NearlyEqual(const FVector4& A, const FVector4& B, const double Tolerance)
	{
		return (
			NearlyEqual(A.X, B.X, Tolerance) &&
			NearlyEqual(A.Y, B.Y, Tolerance) &&
			NearlyEqual(A.Z, B.Z, Tolerance) &&
			NearlyEqual(A.W, B.W, Tolerance));
	}

	template <typename CompilerSafety = void>
	static bool NearlyEqual(const FRotator& A, const FRotator& B, const double Tolerance) { return NearlyEqual(A.Euler(), B.Euler(), Tolerance); }

	template <typename CompilerSafety = void>
	static bool NearlyEqual(const FQuat& A, const FQuat& B, const double Tolerance) { return NearlyEqual(A.Euler(), B.Euler(), Tolerance); }

	template <typename CompilerSafety = void>
	static bool NearlyEqual(const FTransform& A, const FTransform& B, const double Tolerance)
	{
		return (
			NearlyEqual(A.GetLocation(), B.GetLocation(), Tolerance) &&
			NearlyEqual(A.GetRotation(), B.GetRotation(), Tolerance) &&
			NearlyEqual(A.GetScale3D(), B.GetScale3D(), Tolerance));
	}

	template <typename CompilerSafety = void>
	static bool NearlyEqual(const FString& A, const FString& B, const double Tolerance) { return NearlyEqual(A.Len(), B.Len(), Tolerance); }

	template <typename CompilerSafety = void>
	static bool NearlyEqual(const FName& A, const FName& B, const double Tolerance) { return NearlyEqual(A.ToString(), B.ToString(), Tolerance); }

#define PCGEX_UNSUPPORTED_NE(_TYPE) template <typename CompilerSafety = void> static bool NearlyEqual(const _TYPE& A, const _TYPE& B, const double Tolerance) { return false; }
	PCGEX_UNSUPPORTED_PATH_TYPES(PCGEX_UNSUPPORTED_NE)
#undef PCGEX_UNSUPPORTED_NE

#pragma endregion

#pragma region NearlyNotEqual

	template <typename T, typename CompilerSafety = void>
	static bool NearlyNotEqual(const T& A, const T& B, const double Tolerance) { return !NearlyEqual(A, B, Tolerance); }

#pragma endregion

	template <typename T>
	static bool Compare(const EPCGExComparison Method, const T& A, const T& B, const double Tolerance = 0.001)
	{
		switch (Method)
		{
		case EPCGExComparison::StrictlyEqual:
			return StrictlyEqual(A, B);
		case EPCGExComparison::StrictlyNotEqual:
			return StrictlyNotEqual(A, B);
		case EPCGExComparison::EqualOrGreater:
			return EqualOrGreater(A, B);
		case EPCGExComparison::EqualOrSmaller:
			return EqualOrSmaller(A, B);
		case EPCGExComparison::StrictlyGreater:
			return StrictlyGreater(A, B);
		case EPCGExComparison::StrictlySmaller:
			return StrictlySmaller(A, B);
		case EPCGExComparison::NearlyEqual:
			return NearlyEqual(A, B, Tolerance);
		case EPCGExComparison::NearlyNotEqual:
			return NearlyNotEqual(A, B, Tolerance);
		default:
			return false;
		}
	}
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExComparisonSettings
{
	GENERATED_BODY()

	FPCGExComparisonSettings()
	{
	}

	FPCGExComparisonSettings(const FPCGExComparisonSettings& Other):
		Comparison(Other.Comparison),
		Tolerance(Other.Tolerance)
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector OperandA;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector OperandB;

	/** Comparison method. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExComparison Comparison = EPCGExComparison::StrictlyEqual;

	/** Comparison Tolerance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Comparison==EPCGExComparison::NearlyEqual||Comparison==EPCGExComparison::NearlyNotEqual", EditConditionHides, ClampMin=0.001))
	double Tolerance = 0.001;
};


#undef PCGEX_UNSUPPORTED_STRING_TYPES
#undef PCGEX_UNSUPPORTED_PATH_TYPES
