// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGExSettings.h"
#include "Data/PCGExDataCaching.h"

#include "PCGExCompare.generated.h"

#define PCGEX_UNSUPPORTED_STRING_TYPES(MACRO)\
MACRO(FString)\
MACRO(FName)\
MACRO(FSoftObjectPath)\
MACRO(FSoftClassPath)

#define PCGEX_UNSUPPORTED_PATH_TYPES(MACRO)\
MACRO(FSoftObjectPath)\
MACRO(FSoftClassPath)

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Dot Units"))
enum class EPCGExDotUnits : uint8
{
	Raw UMETA(DisplayName = "Normal (-1::1)", Tooltip="Read the value as a raw dot product result"),
	Degrees UMETA(DisplayName = "Degrees", Tooltip="Read the value as degrees"),
};

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

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] String Comparison"))
enum class EPCGExStringComparison : uint8
{
	StrictlyEqual UMETA(DisplayName = " == ", Tooltip="Operand A Strictly Equal to Operand B"),
	StrictlyNotEqual UMETA(DisplayName = " != ", Tooltip="Operand A Strictly Not Equal to Operand B"),
	LengthStrictlyEqual UMETA(DisplayName = " == (Length) ", Tooltip="Operand A Strictly Equal to Operand B"),
	LengthStrictlyUnequal UMETA(DisplayName = " != (Length) ", Tooltip="Operand A Strictly Not Equal to Operand B"),
	LengthEqualOrGreater UMETA(DisplayName = " >= (Length)", Tooltip="Operand A Equal or Greater to Operand B"),
	LengthEqualOrSmaller UMETA(DisplayName = " <= (Length)", Tooltip="Operand A Equal or Smaller to Operand B"),
	StrictlyGreater UMETA(DisplayName = " > (Length)", Tooltip="Operand A Strictly Greater to Operand B"),
	StrictlySmaller UMETA(DisplayName = " < (Length)", Tooltip="Operand A Strictly Smaller to Operand B"),
	LocaleStrictlyGreater UMETA(DisplayName = " > (Locale)", Tooltip="Operand A Locale Strictly Greater to Operand B Locale"),
	LocaleStrictlySmaller UMETA(DisplayName = " < (Locale)", Tooltip="Operand A Locale Strictly Smaller to Operand B Locale"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Bitflag Comparison"))
enum class EPCGExBitflagComparison : uint8
{
	ContainsAny UMETA(DisplayName = "Contains Any", Tooltip="Value & Mask != 0"),
	ContainsAll UMETA(DisplayName = "Contains All", Tooltip="Value & Mask == Mask"),
	IsExactly UMETA(DisplayName = "Is Exactly", Tooltip="Value == Mask"),
	NotContainsAny UMETA(DisplayName = "Not Contains Any", Tooltip="Value & Mask == 0"),
	NotContainsAll UMETA(DisplayName = "Not Contains All", Tooltip="Value & Mask != Mask"),
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

	static FString ToString(const EPCGExBitflagComparison Comparison)
	{
		switch (Comparison)
		{
		case EPCGExBitflagComparison::ContainsAny:
			return " &!=0 ";
		case EPCGExBitflagComparison::ContainsAll:
			return " &== ";
		case EPCGExBitflagComparison::IsExactly:
			return " == ";
		case EPCGExBitflagComparison::NotContainsAny:
			return " &==0 ";
		case EPCGExBitflagComparison::NotContainsAll:
			return " &!= ";
		default:
			return " ?? ";
		}
	}


#pragma region StrictlyEqual

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static bool StrictlyEqual(const T& A, const T& B) { return A == B; }

#pragma endregion

#pragma region StrictlyNotEqual

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static bool StrictlyNotEqual(const T& A, const T& B) { return A != B; }

#pragma endregion

#pragma region EqualOrGreater

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static bool EqualOrGreater(const T& A, const T& B) { return A >= B; }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool EqualOrGreater(const FVector2D& A, const FVector2D& B) { return A.SquaredLength() >= B.SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool EqualOrGreater(const FVector& A, const FVector& B) { return A.SquaredLength() >= B.SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool EqualOrGreater(const FVector4& A, const FVector4& B) { return FVector(A).SquaredLength() >= FVector(B).SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool EqualOrGreater(const FRotator& A, const FRotator& B) { return A.Euler().SquaredLength() >= B.Euler().SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool EqualOrGreater(const FQuat& A, const FQuat& B) { return A.Euler().SquaredLength() >= B.Euler().SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool EqualOrGreater(const FTransform& A, const FTransform& B)
	{
		return (
			EqualOrGreater(A.GetLocation(), B.GetLocation()) &&
			EqualOrGreater(A.GetRotation(), B.GetRotation()) &&
			EqualOrGreater(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static bool EqualOrGreater(const FString& A, const FString& B) { return A.Len() >= B.Len(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool EqualOrGreater(const FName& A, const FName& B) { return EqualOrGreater(A.ToString(), B.ToString()); }

#define PCGEX_UNSUPPORTED_EOG(_TYPE) template <typename CompilerSafety = void> static bool EqualOrGreater(const _TYPE& A, const _TYPE& B) { return false; }
	PCGEX_UNSUPPORTED_PATH_TYPES(PCGEX_UNSUPPORTED_EOG)
#undef PCGEX_UNSUPPORTED_EOG

#pragma endregion

#pragma region EqualOrSmaller

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static bool EqualOrSmaller(const T& A, const T& B) { return A <= B; }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool EqualOrSmaller(const FVector2D& A, const FVector2D& B) { return A.SquaredLength() <= B.SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool EqualOrSmaller(const FVector& A, const FVector& B) { return A.SquaredLength() <= B.SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool EqualOrSmaller(const FVector4& A, const FVector4& B) { return FVector(A).SquaredLength() <= FVector(B).SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool EqualOrSmaller(const FRotator& A, const FRotator& B) { return A.Euler().SquaredLength() <= B.Euler().SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool EqualOrSmaller(const FQuat& A, const FQuat& B) { return A.Euler().SquaredLength() <= B.Euler().SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool EqualOrSmaller(const FTransform& A, const FTransform& B)
	{
		return (
			EqualOrSmaller(A.GetLocation(), B.GetLocation()) &&
			EqualOrSmaller(A.GetRotation(), B.GetRotation()) &&
			EqualOrSmaller(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static bool EqualOrSmaller(const FString& A, const FString& B) { return A.Len() <= B.Len(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool EqualOrSmaller(const FName& A, const FName& B) { return EqualOrSmaller(A.ToString(), B.ToString()); }

#define PCGEX_UNSUPPORTED_EOS(_TYPE) template <typename CompilerSafety = void> static bool EqualOrSmaller(const _TYPE& A, const _TYPE& B) { return false; }
	PCGEX_UNSUPPORTED_PATH_TYPES(PCGEX_UNSUPPORTED_EOS)
#undef PCGEX_UNSUPPORTED_EOS

#pragma endregion

#pragma region EqualOrSmaller

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static bool StrictlyGreater(const T& A, const T& B) { return A > B; }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool StrictlyGreater(const FVector2D& A, const FVector2D& B) { return A.SquaredLength() > B.SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool StrictlyGreater(const FVector& A, const FVector& B) { return A.SquaredLength() > B.SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool StrictlyGreater(const FVector4& A, const FVector4& B) { return FVector(A).SquaredLength() > FVector(B).SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool StrictlyGreater(const FRotator& A, const FRotator& B) { return A.Euler().SquaredLength() > B.Euler().SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool StrictlyGreater(const FQuat& A, const FQuat& B) { return A.Euler().SquaredLength() > B.Euler().SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool StrictlyGreater(const FTransform& A, const FTransform& B)
	{
		return (
			StrictlyGreater(A.GetLocation(), B.GetLocation()) &&
			StrictlyGreater(A.GetRotation(), B.GetRotation()) &&
			StrictlyGreater(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static bool StrictlyGreater(const FString& A, const FString& B) { return A.Len() > B.Len(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool StrictlyGreater(const FName& A, const FName& B) { return StrictlyGreater(A.ToString(), B.ToString()); }

#define PCGEX_UNSUPPORTED_SG(_TYPE) template <typename CompilerSafety = void> static bool StrictlyGreater(const _TYPE& A, const _TYPE& B) { return false; }
	PCGEX_UNSUPPORTED_PATH_TYPES(PCGEX_UNSUPPORTED_SG)
#undef PCGEX_UNSUPPORTED_SG

#pragma endregion

#pragma region StrictlySmaller

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static bool StrictlySmaller(const T& A, const T& B) { return A < B; }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool StrictlySmaller(const FVector2D& A, const FVector2D& B) { return A.SquaredLength() < B.SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool StrictlySmaller(const FVector& A, const FVector& B) { return A.SquaredLength() < B.SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool StrictlySmaller(const FVector4& A, const FVector4& B) { return FVector(A).SquaredLength() < FVector(B).SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool StrictlySmaller(const FRotator& A, const FRotator& B) { return A.Euler().SquaredLength() < B.Euler().SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool StrictlySmaller(const FQuat& A, const FQuat& B) { return A.Euler().SquaredLength() < B.Euler().SquaredLength(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool StrictlySmaller(const FTransform& A, const FTransform& B)
	{
		return (
			StrictlySmaller(A.GetLocation(), B.GetLocation()) &&
			StrictlySmaller(A.GetRotation(), B.GetRotation()) &&
			StrictlySmaller(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static bool StrictlySmaller(const FString& A, const FString& B) { return A.Len() < B.Len(); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool StrictlySmaller(const FName& A, const FName& B) { return EqualOrSmaller(A.ToString(), B.ToString()); }

#define PCGEX_UNSUPPORTED_SS(_TYPE) template <typename CompilerSafety = void> static bool StrictlySmaller(const _TYPE& A, const _TYPE& B) { return false; }
	PCGEX_UNSUPPORTED_PATH_TYPES(PCGEX_UNSUPPORTED_SS)
#undef PCGEX_UNSUPPORTED_SS

#pragma endregion

#pragma region NearlyEqual

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static bool NearlyEqual(const T& A, const T& B, const double Tolerance = 0.001) { return FMath::IsNearlyEqual(A, B, Tolerance); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool NearlyEqual(const FVector2D& A, const FVector2D& B, const double Tolerance)
	{
		return (
			NearlyEqual(A.X, B.X, Tolerance) &&
			NearlyEqual(A.Y, B.Y, Tolerance));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static bool NearlyEqual(const FVector& A, const FVector& B, const double Tolerance)
	{
		return (
			NearlyEqual(A.X, B.X, Tolerance) &&
			NearlyEqual(A.Y, B.Y, Tolerance) &&
			NearlyEqual(A.Z, B.Z, Tolerance));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static bool NearlyEqual(const FVector4& A, const FVector4& B, const double Tolerance)
	{
		return (
			NearlyEqual(A.X, B.X, Tolerance) &&
			NearlyEqual(A.Y, B.Y, Tolerance) &&
			NearlyEqual(A.Z, B.Z, Tolerance) &&
			NearlyEqual(A.W, B.W, Tolerance));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static bool NearlyEqual(const FRotator& A, const FRotator& B, const double Tolerance) { return NearlyEqual(A.Euler(), B.Euler(), Tolerance); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool NearlyEqual(const FQuat& A, const FQuat& B, const double Tolerance) { return NearlyEqual(A.Euler(), B.Euler(), Tolerance); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool NearlyEqual(const FTransform& A, const FTransform& B, const double Tolerance)
	{
		return (
			NearlyEqual(A.GetLocation(), B.GetLocation(), Tolerance) &&
			NearlyEqual(A.GetRotation(), B.GetRotation(), Tolerance) &&
			NearlyEqual(A.GetScale3D(), B.GetScale3D(), Tolerance));
	}

	template <typename CompilerSafety = void>
	FORCEINLINE static bool NearlyEqual(const FString& A, const FString& B, const double Tolerance) { return NearlyEqual(A.Len(), B.Len(), Tolerance); }

	template <typename CompilerSafety = void>
	FORCEINLINE static bool NearlyEqual(const FName& A, const FName& B, const double Tolerance) { return NearlyEqual(A.ToString(), B.ToString(), Tolerance); }

#define PCGEX_UNSUPPORTED_NE(_TYPE) template <typename CompilerSafety = void> static bool NearlyEqual(const _TYPE& A, const _TYPE& B, const double Tolerance) { return false; }
	PCGEX_UNSUPPORTED_PATH_TYPES(PCGEX_UNSUPPORTED_NE)
#undef PCGEX_UNSUPPORTED_NE

#pragma endregion

#pragma region NearlyNotEqual

	template <typename T, typename CompilerSafety = void>
	FORCEINLINE static bool NearlyNotEqual(const T& A, const T& B, const double Tolerance) { return !NearlyEqual(A, B, Tolerance); }

#pragma endregion

	template <typename T>
	FORCEINLINE static bool Compare(const EPCGExComparison Method, const T& A, const T& B, const double Tolerance = 0.001)
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

	FORCEINLINE static bool Compare(const EPCGExBitflagComparison Method, const int64& Value, const int64& Mask)
	{
		switch (Method)
		{
		case EPCGExBitflagComparison::ContainsAny:
			return ((Value & Mask) != 0);
		case EPCGExBitflagComparison::ContainsAll:
			return ((Value & Mask) == Mask);
		case EPCGExBitflagComparison::IsExactly:
			return (Value == Mask);
		case EPCGExBitflagComparison::NotContainsAny:
			return ((Value & Mask) == 0);
		case EPCGExBitflagComparison::NotContainsAll:
			return ((Value & Mask) != Mask);
		default: return false;
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

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector OperandA;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector OperandB;

	/** Comparison method. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExComparison Comparison = EPCGExComparison::StrictlyEqual;

	/** Comparison Tolerance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Comparison==EPCGExComparison::NearlyEqual||Comparison==EPCGExComparison::NearlyNotEqual", EditConditionHides, ClampMin=0.001))
	double Tolerance = 0.001;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExDotComparisonSettings
{
	GENERATED_BODY()

	FPCGExDotComparisonSettings()
	{
	}

	/** Comparison of the Dot value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExComparison Comparison = EPCGExComparison::EqualOrGreater;

	/** If enabled, the dot product will be made absolute before testing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDotUnits DotUnits = EPCGExDotUnits::Raw;

	/** If enabled, the dot product will be made absolute before testing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bUnsignedDot = false;

	/** Type of Dot value source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFetchType DotValue = EPCGExFetchType::Constant;

	/** Dot value use for comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="DotValue==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector DotAttribute;

	/** Dot value use for comparison (In raw -1/1 range) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="DotValue==EPCGExFetchType::Constant && DotUnits==EPCGExDotUnits::Raw", EditConditionHides, ClampMin=-1, ClampMax=1))
	double DotConstantRaw = 1;

	/** Dot value use for comparison (In degrees) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="DotValue==EPCGExFetchType::Constant && DotUnits==EPCGExDotUnits::Degrees", EditConditionHides, ClampMin=0, ClampMax=180, Units="Degrees"))
	double DotConstantDegrees = 0;

	/** Tolerance for dot comparison. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="(Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual) && DotUnits==EPCGExDotUnits::Raw", EditConditionHides, ClampMin=0, ClampMax=1))
	double DotToleranceRaw = 0.1;

	/** Tolerance for dot comparison. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="(Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual) && DotUnits==EPCGExDotUnits::Degrees", EditConditionHides, ClampMin=0, ClampMax=180, Units="Degrees"))
	double DotToleranceDegrees = 0.1;

	bool bUseLocalDot = false;
	PCGExDataCaching::FCache<double>* LocalOperand = nullptr;

	bool Init(const FPCGContext* InContext, PCGExDataCaching::FPool* InPrimaryDataCache)
	{
		bUseLocalDot = DotValue == EPCGExFetchType::Attribute;

		if (bUseLocalDot)
		{
			LocalOperand = InPrimaryDataCache->GetOrCreateGetter<double>(DotAttribute);
			if (!LocalOperand)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Dot attribute: {0}."), FText::FromName(DotAttribute.GetName())));
				return false;
			}
		}

		if (DotUnits == EPCGExDotUnits::Degrees)
		{
			DotToleranceRaw = PCGExMath::DegreesToDot(DotToleranceDegrees);
			DotConstantRaw = PCGExMath::DegreesToDot(DotConstantDegrees);
		}

		return true;
	}

	double GetDot(const int32 PointIndex) const
	{
		if (bUseLocalDot)
		{
			switch (DotUnits)
			{
			default: ;
			case EPCGExDotUnits::Raw:
				return LocalOperand->Values[PointIndex];
			case EPCGExDotUnits::Degrees:
				return PCGExMath::DegreesToDot(LocalOperand->Values[PointIndex]);
			}
		}
		return DotToleranceRaw;
	}

	bool Test(const double A, const double B) const
	{
		return PCGExCompare::Compare(Comparison, A, B, DotToleranceRaw);
	}
};

/*
UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Bitflag 64"))
enum class EPCGExBitflag64 : int64
{
	None    = 0,
	Flag_1  = 1ULL << 0 UMETA(DisplayName = "Alpha"),
	Flag_2  = 1ULL << 1 UMETA(DisplayName = "Beta"),
	Flag_3  = 1ULL << 2 UMETA(DisplayName = "Gamma"),
	Flag_4  = 1ULL << 3 UMETA(DisplayName = "Delta"),
	Flag_5  = 1ULL << 4 UMETA(DisplayName = "Epsilon"),
	Flag_6  = 1ULL << 5 UMETA(DisplayName = "Zeta"),
	Flag_7  = 1ULL << 6 UMETA(DisplayName = "Eta"),
	Flag_8  = 1ULL << 7 UMETA(DisplayName = "Theta"),
	Flag_9  = 1ULL << 8 UMETA(DisplayName = "Iota"),
	Flag_10 = 1ULL << 9 UMETA(DisplayName = "Kappa"),
	Flag_11 = 1ULL << 10 UMETA(DisplayName = "Lambda"),
	Flag_12 = 1ULL << 11 UMETA(DisplayName = "Mu"),
	Flag_13 = 1ULL << 12 UMETA(DisplayName = "Nu"),
	Flag_14 = 1ULL << 13 UMETA(DisplayName = "Xi"),
	Flag_15 = 1ULL << 14 UMETA(DisplayName = "Omicron"),
	Flag_16 = 1ULL << 15 UMETA(DisplayName = "Pi"),
	Flag_17 = 1ULL << 16 UMETA(DisplayName = "Rho"),
	Flag_18 = 1ULL << 17 UMETA(DisplayName = "Sigma"),
	Flag_19 = 1ULL << 18 UMETA(DisplayName = "Tau"),
	Flag_20 = 1ULL << 19 UMETA(DisplayName = "Upsilon"),
	Flag_21 = 1ULL << 20 UMETA(DisplayName = "Phi"),
	Flag_22 = 1ULL << 21 UMETA(DisplayName = "Chi"),
	Flag_23 = 1ULL << 22 UMETA(DisplayName = "Psi"),
	Flag_24 = 1ULL << 23 UMETA(DisplayName = "Omega"),
	Flag_25 = 1ULL << 24 UMETA(DisplayName = "Ares"),
	Flag_26 = 1ULL << 25 UMETA(DisplayName = "Zeus"),
	Flag_27 = 1ULL << 26 UMETA(DisplayName = "Hera"),
	Flag_28 = 1ULL << 27 UMETA(DisplayName = "Apollo"),
	Flag_29 = 1ULL << 28 UMETA(DisplayName = "Hermes"),
	Flag_30 = 1ULL << 29 UMETA(DisplayName = "Athena"),
	Flag_31 = 1ULL << 30 UMETA(DisplayName = "Artemis"),
	Flag_32 = 1ULL << 31 UMETA(DisplayName = "Demeter"),
	Flag_33 = 1ULL << 32 UMETA(DisplayName = "Dionysus"),
	Flag_34 = 1ULL << 33 UMETA(DisplayName = "Hades"),
	Flag_35 = 1ULL << 34 UMETA(DisplayName = "Hephaestus"),
	Flag_36 = 1ULL << 35 UMETA(DisplayName = "Hera"),
	Flag_37 = 1ULL << 36 UMETA(DisplayName = "Hestia"),
	Flag_38 = 1ULL << 37 UMETA(DisplayName = "Poseidon"),
	Flag_39 = 1ULL << 38 UMETA(DisplayName = "Janus"),
	Flag_40 = 1ULL << 39 UMETA(DisplayName = "Mars"),
	Flag_41 = 1ULL << 40 UMETA(DisplayName = "Venus"),
	Flag_42 = 1ULL << 41 UMETA(DisplayName = "Jupiter"),
	Flag_43 = 1ULL << 42 UMETA(DisplayName = "Saturn"),
	Flag_44 = 1ULL << 43 UMETA(DisplayName = "Neptune"),
	Flag_45 = 1ULL << 44 UMETA(DisplayName = "Pluto"),
	Flag_46 = 1ULL << 45 UMETA(DisplayName = "Vesta"),
	Flag_47 = 1ULL << 46 UMETA(DisplayName = "Mercury"),
	Flag_48 = 1ULL << 47 UMETA(DisplayName = "Sol"),
	Flag_49 = 1ULL << 48 UMETA(DisplayName = "Luna"),
	Flag_50 = 1ULL << 49 UMETA(DisplayName = "Terra"),
	Flag_51 = 1ULL << 50 UMETA(DisplayName = "Vulcan"),
	Flag_52 = 1ULL << 51 UMETA(DisplayName = "Juno"),
	Flag_53 = 1ULL << 52 UMETA(DisplayName = "Ceres"),
	Flag_54 = 1ULL << 53 UMETA(DisplayName = "Minerva"),
	Flag_55 = 1ULL << 54 UMETA(DisplayName = "Bacchus"),
	Flag_56 = 1ULL << 55 UMETA(DisplayName = "Aurora"),
	Flag_57 = 1ULL << 56 UMETA(DisplayName = "Flora"),
	Flag_58 = 1ULL << 57 UMETA(DisplayName = "Faunus"),
	Flag_59 = 1ULL << 58 UMETA(DisplayName = "Iris"),
	Flag_60 = 1ULL << 59 UMETA(DisplayName = "Mithras"),
	Flag_61 = 1ULL << 60 UMETA(DisplayName = "Fortuna"),
	Flag_62 = 1ULL << 61 UMETA(DisplayName = "Bellona"),
	Flag_63 = 1ULL << 62 UMETA(DisplayName = "Fides"),
	Flag_64 = 1ULL << 63 UMETA(DisplayName = "Pax"),
};
ENUM_CLASS_FLAGS(EPCGExBitflag64)
*/

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 0-8 Bits Range"))
enum class EPCGExBitflag8_1 : uint8
{
	None   = 0,
	Flag_1 = 1 << 0 UMETA(DisplayName = "(0) Alpha"),
	Flag_2 = 1 << 1 UMETA(DisplayName = "(1) Beta"),
	Flag_3 = 1 << 2 UMETA(DisplayName = "(2) Gamma"),
	Flag_4 = 1 << 3 UMETA(DisplayName = "(3) Delta"),
	Flag_5 = 1 << 4 UMETA(DisplayName = "(4) Epsilon"),
	Flag_6 = 1 << 5 UMETA(DisplayName = "(5) Zeta"),
	Flag_7 = 1 << 6 UMETA(DisplayName = "(6) Eta"),
	Flag_8 = 1 << 7 UMETA(DisplayName = "(7) Theta"),
};

ENUM_CLASS_FLAGS(EPCGExBitflag8_1)
typedef TEnumAsByte<EPCGExBitflag8_1> EPCGExBitflag8_1Bitmask;

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 8-16 Bits Range"))
enum class EPCGExBitflag8_2 : uint8
{
	None    = 0,
	Flag_9  = 1 << 0 UMETA(DisplayName = "(9) Iota"),
	Flag_10 = 1 << 1 UMETA(DisplayName = "(10) Kappa"),
	Flag_11 = 1 << 2 UMETA(DisplayName = "(11) Lambda"),
	Flag_12 = 1 << 3 UMETA(DisplayName = "(12) Mu"),
	Flag_13 = 1 << 4 UMETA(DisplayName = "(13) Nu"),
	Flag_14 = 1 << 5 UMETA(DisplayName = "(14) Xi"),
	Flag_15 = 1 << 6 UMETA(DisplayName = "(15) Omicron"),
	Flag_16 = 1 << 7 UMETA(DisplayName = "(16) Pi"),
};

ENUM_CLASS_FLAGS(EPCGExBitflag8_2)
typedef TEnumAsByte<EPCGExBitflag8_2> EPCGExBitflag8_2Bitmask;

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 16-24 Bits Range"))
enum class EPCGExBitflag8_3 : uint8
{
	None    = 0,
	Flag_17 = 1 << 0 UMETA(DisplayName = "(17) Rho"),
	Flag_18 = 1 << 1 UMETA(DisplayName = "(18) Sigma"),
	Flag_19 = 1 << 2 UMETA(DisplayName = "(19) Tau"),
	Flag_20 = 1 << 3 UMETA(DisplayName = "(20) Upsilon"),
	Flag_21 = 1 << 4 UMETA(DisplayName = "(21) Phi"),
	Flag_22 = 1 << 5 UMETA(DisplayName = "(22) Chi"),
	Flag_23 = 1 << 6 UMETA(DisplayName = "(23) Psi"),
	Flag_24 = 1 << 7 UMETA(DisplayName = "(24) Omega"),
};

ENUM_CLASS_FLAGS(EPCGExBitflag8_3)
typedef TEnumAsByte<EPCGExBitflag8_3> EPCGExBitflag8_3Bitmask;

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 24-32 Bits Range"))
enum class EPCGExBitflag8_4 : uint8
{
	None    = 0,
	Flag_25 = 1 << 0 UMETA(DisplayName = "(25) Ares"),
	Flag_26 = 1 << 1 UMETA(DisplayName = "(26) Zeus"),
	Flag_27 = 1 << 2 UMETA(DisplayName = "(27) Hera"),
	Flag_28 = 1 << 3 UMETA(DisplayName = "(28) Apollo"),
	Flag_29 = 1 << 4 UMETA(DisplayName = "(29) Hermes"),
	Flag_30 = 1 << 5 UMETA(DisplayName = "(30) Athena"),
	Flag_31 = 1 << 6 UMETA(DisplayName = "(31) Artemis"),
	Flag_32 = 1 << 7 UMETA(DisplayName = "(32) Demeter"),
};

ENUM_CLASS_FLAGS(EPCGExBitflag8_4)
typedef TEnumAsByte<EPCGExBitflag8_4> EPCGExBitflag8_4Bitmask;

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 32-40 Bits Range"))
enum class EPCGExBitflag8_5 : uint8
{
	None    = 0,
	Flag_33 = 1 << 0 UMETA(DisplayName = "(33) Dionysus"),
	Flag_34 = 1 << 1 UMETA(DisplayName = "(34) Hades"),
	Flag_35 = 1 << 2 UMETA(DisplayName = "(35) Hephaestus"),
	Flag_36 = 1 << 3 UMETA(DisplayName = "(36) Hera"),
	Flag_37 = 1 << 4 UMETA(DisplayName = "(37) Hestia"),
	Flag_38 = 1 << 5 UMETA(DisplayName = "(38) Poseidon"),
	Flag_39 = 1 << 6 UMETA(DisplayName = "(39) Janus"),
	Flag_40 = 1 << 7 UMETA(DisplayName = "(40) Mars"),
};

ENUM_CLASS_FLAGS(EPCGExBitflag8_5)
typedef TEnumAsByte<EPCGExBitflag8_5> EPCGExBitflag8_5Bitmask;

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 40-48 Bits Range"))
enum class EPCGExBitflag8_6 : uint8
{
	None    = 0,
	Flag_41 = 1 << 0 UMETA(DisplayName = "(41) Venus"),
	Flag_42 = 1 << 1 UMETA(DisplayName = "(42) Jupiter"),
	Flag_43 = 1 << 2 UMETA(DisplayName = "(43) Saturn"),
	Flag_44 = 1 << 3 UMETA(DisplayName = "(44) Neptune"),
	Flag_45 = 1 << 4 UMETA(DisplayName = "(45) Pluto"),
	Flag_46 = 1 << 5 UMETA(DisplayName = "(46) Vesta"),
	Flag_47 = 1 << 6 UMETA(DisplayName = "(47) Mercury"),
	Flag_48 = 1 << 7 UMETA(DisplayName = "(48) Sol"),
};

ENUM_CLASS_FLAGS(EPCGExBitflag8_6)
typedef TEnumAsByte<EPCGExBitflag8_6> EPCGExBitflag8_6Bitmask;

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 48-56 Bits Range"))
enum class EPCGExBitflag8_7 : uint8
{
	None    = 0,
	Flag_49 = 1 << 0 UMETA(DisplayName = "(49) Luna"),
	Flag_50 = 1 << 1 UMETA(DisplayName = "(50) Terra"),
	Flag_51 = 1 << 2 UMETA(DisplayName = "(51) Vulcan"),
	Flag_52 = 1 << 3 UMETA(DisplayName = "(52) Juno"),
	Flag_53 = 1 << 4 UMETA(DisplayName = "(53) Ceres"),
	Flag_54 = 1 << 5 UMETA(DisplayName = "(54) Minerva"),
	Flag_55 = 1 << 6 UMETA(DisplayName = "(55) Bacchus"),
	Flag_56 = 1 << 7 UMETA(DisplayName = "(56) Aurora"),
};

ENUM_CLASS_FLAGS(EPCGExBitflag8_7)
typedef TEnumAsByte<EPCGExBitflag8_7> EPCGExBitflag8_7Bitmask;

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 56-64 Bits Range"))
enum class EPCGExBitflag8_8 : uint8
{
	None    = 0,
	Flag_57 = 1 << 0 UMETA(DisplayName = "(57) Flora"),
	Flag_58 = 1 << 1 UMETA(DisplayName = "(58) Faunus"),
	Flag_59 = 1 << 2 UMETA(DisplayName = "(59) Iris"),
	Flag_60 = 1 << 3 UMETA(DisplayName = "(60) Mithras"),
	Flag_61 = 1 << 4 UMETA(DisplayName = "(61) Fortuna"),
	Flag_62 = 1 << 5 UMETA(DisplayName = "(62) Bellona"),
	Flag_63 = 1 << 6 UMETA(DisplayName = "(63) Fides"),
	Flag_64 = 1 << 7 UMETA(DisplayName = "(64) Pax"),
};

ENUM_CLASS_FLAGS(EPCGExBitflag8_8)
typedef TEnumAsByte<EPCGExBitflag8_8> EPCGExBitflag8_8Bitmask;


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExCompositeBitflagValue
{
	GENERATED_BODY()

	FPCGExCompositeBitflagValue()
	{
	}

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	//bool bWriteToRange1 = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitflag8_1", DisplayName="0-8 Bits"))
	uint8 Range1 = 0;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	//bool bWriteToRange2 = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitflag8_2", DisplayName="8-16 Bits"))
	uint8 Range2 = 0;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	//bool bWriteToRange3 = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitflag8_3", DisplayName="16-24 Bits"))
	uint8 Range3 = 0;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	//bool bWriteToRange4 = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitflag8_4", DisplayName="24-32 Bits"))
	uint8 Range4 = 0;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	//bool bWriteToRange5 = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitflag8_5", DisplayName="32-40 Bits"))
	uint8 Range5 = 0;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	//bool bWriteToRange6 = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitflag8_6", DisplayName="40-48 Bits"))
	uint8 Range6 = 0;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	//bool bWriteToRange7 = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitflag8_7", DisplayName="48-56 Bits"))
	uint8 Range7 = 0;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	//bool bWriteToRange8 = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitflag8_8", DisplayName="56-64 Bits"))
	uint8 Range8 = 0;

	int64 GetComposite() const
	{
		int64 Composite = 0;

		/*
		Composite |= (bWriteToRange1 ? static_cast<int64>(Range1) : 0) << 0;
		Composite |= (bWriteToRange2 ? static_cast<int64>(Range2) : 0) << 8;
		Composite |= (bWriteToRange3 ? static_cast<int64>(Range3) : 0) << 16;
		Composite |= (bWriteToRange4 ? static_cast<int64>(Range4) : 0) << 24;
		Composite |= (bWriteToRange5 ? static_cast<int64>(Range5) : 0) << 32;
		Composite |= (bWriteToRange6 ? static_cast<int64>(Range6) : 0) << 40;
		Composite |= (bWriteToRange7 ? static_cast<int64>(Range7) : 0) << 48;
		Composite |= (bWriteToRange8 ? static_cast<int64>(Range8) : 0) << 56;
		*/

		Composite |= static_cast<int64>(Range1) << 0;
		Composite |= static_cast<int64>(Range2) << 8;
		Composite |= static_cast<int64>(Range3) << 16;
		Composite |= static_cast<int64>(Range4) << 24;
		Composite |= static_cast<int64>(Range5) << 32;
		Composite |= static_cast<int64>(Range6) << 40;
		Composite |= static_cast<int64>(Range7) << 48;
		Composite |= static_cast<int64>(Range8) << 56;

		return Composite;
	}
};

#undef PCGEX_UNSUPPORTED_STRING_TYPES
#undef PCGEX_UNSUPPORTED_PATH_TYPES
