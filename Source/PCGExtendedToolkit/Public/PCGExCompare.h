// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGExDetails.h"
#include "Data/PCGExData.h"

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
	Contains UMETA(DisplayName = " Contains ", Tooltip="Operand A contains Operand B"),
	StartsWith UMETA(DisplayName = " Starts With ", Tooltip="Operand A starts with Operand B"),
	EndsWith UMETA(DisplayName = " Ends With ", Tooltip="Operand A ends with Operand B"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Bitflag Comparison"))
enum class EPCGExBitflagComparison : uint8
{
	MatchPartial UMETA(DisplayName = "Match (any)", Tooltip="Value & Mask != 0 (At least some flags in the mask are set)"),
	MatchFull UMETA(DisplayName = "Match (all)", Tooltip="Value & Mask == Mask (All the flags in the mask are set)"),
	MatchStrict UMETA(DisplayName = "Match (strict)", Tooltip="Value == Mask (Flags strictly equals mask)"),
	NoMatchPartial UMETA(DisplayName = "No match (any)", Tooltip="Value & Mask == 0 (Flags does not contains any from mask)"),
	NoMatchFull UMETA(DisplayName = "No match (all)", Tooltip="Value & Mask != Mask (Flags does not contains the mask)"),
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
		case EPCGExBitflagComparison::MatchPartial:
			return " Any ";
		case EPCGExBitflagComparison::MatchFull:
			return " All ";
		case EPCGExBitflagComparison::MatchStrict:
			return " Exactly ";
		case EPCGExBitflagComparison::NoMatchPartial:
			return " Not Any ";
		case EPCGExBitflagComparison::NoMatchFull:
			return " Not All ";
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

	FORCEINLINE static bool Compare(const EPCGExBitflagComparison Method, const int64& Flags, const int64& Mask)
	{
		switch (Method)
		{
		case EPCGExBitflagComparison::MatchPartial:
			return ((Flags & Mask) != 0);
		case EPCGExBitflagComparison::MatchFull:
			return ((Flags & Mask) == Mask);
		case EPCGExBitflagComparison::MatchStrict:
			return (Flags == Mask);
		case EPCGExBitflagComparison::NoMatchPartial:
			return ((Flags & Mask) == 0);
		case EPCGExBitflagComparison::NoMatchFull:
			return ((Flags & Mask) != Mask);
		default: return false;
		}
	}
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExComparisonDetails
{
	GENERATED_BODY()

	FPCGExComparisonDetails()
	{
	}

	FPCGExComparisonDetails(const FPCGExComparisonDetails& Other):
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


UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Direction Check Mode"))
enum class EPCGExDirectionCheckMode : uint8
{
	Dot UMETA(DisplayName = "Dot (Precise)", Tooltip="Extensive comparison using Dot product"),
	Hash UMETA(DisplayName = "Hash (Fast)", Tooltip="Simplified check using hash comparison with a destructive tolerance"),
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExVectorHashComparisonDetails
{
	GENERATED_BODY()

	FPCGExVectorHashComparisonDetails()
	{
	}

	/** Type of Tolerance value source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFetchType HashToleranceValue = EPCGExFetchType::Constant;

	/** Tolerance value use for comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="HashToleranceValue==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector HashToleranceAttribute;

	/** Tolerance value use for comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditConditionHides, ClampMin=0.00001))
	double HashToleranceConstant = 0.001;

	FVector CWTolerance = FVector::ZeroVector;

	bool bUseLocalTolerance = false;
	PCGExData::FCache<double>* LocalOperand = nullptr;

	bool Init(const FPCGContext* InContext, PCGExData::FFacade* InPrimaryDataFacade)
	{
		bUseLocalTolerance = HashToleranceValue == EPCGExFetchType::Attribute;

		if (bUseLocalTolerance)
		{
			LocalOperand = InPrimaryDataFacade->GetOrCreateGetter<double>(HashToleranceAttribute);
			if (!LocalOperand)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Hash Tolerance attribute: {0}."), FText::FromName(HashToleranceAttribute.GetName())));
				return false;
			}
		}

		CWTolerance = FVector(1 / HashToleranceConstant);
		return true;
	}

	FVector GetCWTolerance(const int32 PointIndex) const
	{
		return bUseLocalTolerance ? FVector(1 / LocalOperand->Values[PointIndex]) : CWTolerance;
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExDotComparisonDetails
{
	GENERATED_BODY()

	FPCGExDotComparisonDetails()
	{
	}

	/** Comparison of the Dot value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExComparison Comparison = EPCGExComparison::EqualOrGreater;

	/** If enabled, the dot product will be made absolute before testing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExDotUnits DotUnits = EPCGExDotUnits::Raw;

	/** If enabled, the dot product will be made absolute before testing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bUnsignedDot = false;

	/** Type of Dot value source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExFetchType DotValue = EPCGExFetchType::Constant;

	/** Dot value use for comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="DotValue==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector DotOrDegreesAttribute;

	/** Dot value use for comparison (In raw -1/1 range) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="DotValue==EPCGExFetchType::Constant && DotUnits==EPCGExDotUnits::Raw", EditConditionHides, ClampMin=-1, ClampMax=1))
	double DotConstant = 1;

	/** Tolerance for dot comparison. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="(Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual) && DotUnits==EPCGExDotUnits::Raw", EditConditionHides, ClampMin=0, ClampMax=1))
	double DotTolerance = 0.1;

	/** Dot value use for comparison (In degrees) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="DotValue==EPCGExFetchType::Constant && DotUnits==EPCGExDotUnits::Degrees", EditConditionHides, ClampMin=0, ClampMax=180, Units="Degrees"))
	double DegreesConstant = 0;

	/** Tolerance for dot comparison. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="(Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual) && DotUnits==EPCGExDotUnits::Degrees", EditConditionHides, ClampMin=0, ClampMax=180, Units="Degrees"))
	double DegreesTolerance = 0.1;

	bool bUseLocalDot = false;
	PCGExData::FCache<double>* LocalOperand = nullptr;

	bool Init(const FPCGContext* InContext, PCGExData::FFacade* InPrimaryDataCache)
	{
		bUseLocalDot = DotValue == EPCGExFetchType::Attribute;

		if (bUseLocalDot)
		{
			LocalOperand = InPrimaryDataCache->GetOrCreateGetter<double>(DotOrDegreesAttribute);
			if (!LocalOperand)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Dot attribute: {0}."), FText::FromName(DotOrDegreesAttribute.GetName())));
				return false;
			}
		}

		if (DotUnits == EPCGExDotUnits::Degrees)
		{
			DotTolerance = PCGExMath::DegreesToDot(DegreesTolerance * 0.5);
			DotConstant = PCGExMath::DegreesToDotForComparison(DegreesConstant * 0.5);
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
				return PCGExMath::DegreesToDotForComparison(LocalOperand->Values[PointIndex] * 0.5);
			}
		}
		return DotConstant;
	}

	bool Test(const double A, const double B) const
	{
		return PCGExCompare::Compare(Comparison, A, B, DotTolerance);
	}
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Bit Operation"))
enum class EPCGExBitOp : uint8
{
	Set UMETA(DisplayName = "=", ToolTip="(Flags = Mask) Set the bit with the specified value."),
	AND UMETA(DisplayName = "AND", ToolTip="(Flags &= Mask) Output true if boths bits == 1, otherwise false."),
	OR UMETA(DisplayName = "OR", ToolTip="(Flags |= Mask) Output true if any of the bits == 1, otherwise false."),
	NOT UMETA(DisplayName = "NOT", ToolTip="(Flags &= ~Mask) Like AND, but inverts the masks."),
	XOR UMETA(DisplayName = "XOR", ToolTip="(Flags ^= Mask) Invert the flag bit where the mask == 1."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Bitmask Source"))
enum class EPCGExBitmaskMode : uint8
{
	Direct UMETA(DisplayName = "Direct", ToolTip="Used for easy override mostly"),
	Individual UMETA(DisplayName = "Individual", ToolTip="Use an array to manually set the bits"),
	Composite UMETA(DisplayName = "Composite", ToolTip="Use a ton of dropdown to set the bits"),
};

#pragma region Bitmasks


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
enum class EPCGExBitmask8_00_08 : uint8
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

ENUM_CLASS_FLAGS(EPCGExBitmask8_00_08)
using EPCGExBitmask8_00_08Bitmask = TEnumAsByte<EPCGExBitmask8_00_08>;

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 8-16 Bits Range"))
enum class EPCGExBitmask8_08_16 : uint8
{
	None    = 0,
	Flag_9  = 1 << 0 UMETA(DisplayName = "(8) Iota"),
	Flag_10 = 1 << 1 UMETA(DisplayName = "(9) Kappa"),
	Flag_11 = 1 << 2 UMETA(DisplayName = "(10) Lambda"),
	Flag_12 = 1 << 3 UMETA(DisplayName = "(11) Mu"),
	Flag_13 = 1 << 4 UMETA(DisplayName = "(12) Nu"),
	Flag_14 = 1 << 5 UMETA(DisplayName = "(13) Xi"),
	Flag_15 = 1 << 6 UMETA(DisplayName = "(14) Omicron"),
	Flag_16 = 1 << 7 UMETA(DisplayName = "(15) Pi"),
};

ENUM_CLASS_FLAGS(EPCGExBitmask8_08_16)
using EPCGExBitmask8_08_16Bitmask = TEnumAsByte<EPCGExBitmask8_08_16>;

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 16-24 Bits Range"))
enum class EPCGExBitmask8_16_24 : uint8
{
	None    = 0,
	Flag_17 = 1 << 0 UMETA(DisplayName = "(16) Rho"),
	Flag_18 = 1 << 1 UMETA(DisplayName = "(17) Sigma"),
	Flag_19 = 1 << 2 UMETA(DisplayName = "(18) Tau"),
	Flag_20 = 1 << 3 UMETA(DisplayName = "(19) Upsilon"),
	Flag_21 = 1 << 4 UMETA(DisplayName = "(20) Phi"),
	Flag_22 = 1 << 5 UMETA(DisplayName = "(21) Chi"),
	Flag_23 = 1 << 6 UMETA(DisplayName = "(22) Psi"),
	Flag_24 = 1 << 7 UMETA(DisplayName = "(23) Omega"),
};

ENUM_CLASS_FLAGS(EPCGExBitmask8_16_24)
using EPCGExBitmask8_16_24Bitmask = TEnumAsByte<EPCGExBitmask8_16_24>;

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 24-32 Bits Range"))
enum class EPCGExBitmask8_24_32 : uint8
{
	None    = 0,
	Flag_25 = 1 << 0 UMETA(DisplayName = "(24) Ares"),
	Flag_26 = 1 << 1 UMETA(DisplayName = "(25) Zeus"),
	Flag_27 = 1 << 2 UMETA(DisplayName = "(26) Hera"),
	Flag_28 = 1 << 3 UMETA(DisplayName = "(27) Apollo"),
	Flag_29 = 1 << 4 UMETA(DisplayName = "(28) Hermes"),
	Flag_30 = 1 << 5 UMETA(DisplayName = "(29) Athena"),
	Flag_31 = 1 << 6 UMETA(DisplayName = "(30) Artemis"),
	Flag_32 = 1 << 7 UMETA(DisplayName = "(31) Demeter"),
};

ENUM_CLASS_FLAGS(EPCGExBitmask8_24_32)
using EPCGExBitmask8_24_32Bitmask = TEnumAsByte<EPCGExBitmask8_24_32>;

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 32-40 Bits Range"))
enum class EPCGExBitmask8_32_40 : uint8
{
	None    = 0,
	Flag_33 = 1 << 0 UMETA(DisplayName = "(32) Dionysus"),
	Flag_34 = 1 << 1 UMETA(DisplayName = "(33) Hades"),
	Flag_35 = 1 << 2 UMETA(DisplayName = "(34) Hephaestus"),
	Flag_36 = 1 << 3 UMETA(DisplayName = "(35) Hera"),
	Flag_37 = 1 << 4 UMETA(DisplayName = "(36) Hestia"),
	Flag_38 = 1 << 5 UMETA(DisplayName = "(37) Poseidon"),
	Flag_39 = 1 << 6 UMETA(DisplayName = "(38) Janus"),
	Flag_40 = 1 << 7 UMETA(DisplayName = "(39) Mars"),
};

ENUM_CLASS_FLAGS(EPCGExBitmask8_32_40)
using EPCGExBitmask8_32_40Bitmask = TEnumAsByte<EPCGExBitmask8_32_40>;

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 40-48 Bits Range"))
enum class EPCGExBitmask8_40_48 : uint8
{
	None    = 0,
	Flag_41 = 1 << 0 UMETA(DisplayName = "(40) Venus"),
	Flag_42 = 1 << 1 UMETA(DisplayName = "(41) Jupiter"),
	Flag_43 = 1 << 2 UMETA(DisplayName = "(42) Saturn"),
	Flag_44 = 1 << 3 UMETA(DisplayName = "(43) Neptune"),
	Flag_45 = 1 << 4 UMETA(DisplayName = "(44) Pluto"),
	Flag_46 = 1 << 5 UMETA(DisplayName = "(45) Vesta"),
	Flag_47 = 1 << 6 UMETA(DisplayName = "(46) Mercury"),
	Flag_48 = 1 << 7 UMETA(DisplayName = "(47) Sol"),
};

ENUM_CLASS_FLAGS(EPCGExBitmask8_40_48)
using EPCGExBitmask8_40_48Bitmask = TEnumAsByte<EPCGExBitmask8_40_48>;

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 48-56 Bits Range"))
enum class EPCGExBitmask8_48_56 : uint8
{
	None    = 0,
	Flag_49 = 1 << 0 UMETA(DisplayName = "(48) Luna"),
	Flag_50 = 1 << 1 UMETA(DisplayName = "(49) Terra"),
	Flag_51 = 1 << 2 UMETA(DisplayName = "(50) Vulcan"),
	Flag_52 = 1 << 3 UMETA(DisplayName = "(51) Juno"),
	Flag_53 = 1 << 4 UMETA(DisplayName = "(52) Ceres"),
	Flag_54 = 1 << 5 UMETA(DisplayName = "(53) Minerva"),
	Flag_55 = 1 << 6 UMETA(DisplayName = "(54) Bacchus"),
	Flag_56 = 1 << 7 UMETA(DisplayName = "(55) Aurora"),
};

ENUM_CLASS_FLAGS(EPCGExBitmask8_48_56)
using EPCGExBitmask8_48_56Bitmask = TEnumAsByte<EPCGExBitmask8_48_56>;

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 56-64 Bits Range"))
enum class EPCGExBitmask8_56_64 : uint8
{
	None    = 0,
	Flag_57 = 1 << 0 UMETA(DisplayName = "(56) Flora"),
	Flag_58 = 1 << 1 UMETA(DisplayName = "(57) Faunus"),
	Flag_59 = 1 << 2 UMETA(DisplayName = "(58) Iris"),
	Flag_60 = 1 << 3 UMETA(DisplayName = "(59) Mithras"),
	Flag_61 = 1 << 4 UMETA(DisplayName = "(60) Fortuna"),
	Flag_62 = 1 << 5 UMETA(DisplayName = "(61) Bellona"),
	Flag_63 = 1 << 6 UMETA(DisplayName = "(62) Fides"),
	Flag_64 = 1 << 7 UMETA(DisplayName = "(63) Pax"),
};

ENUM_CLASS_FLAGS(EPCGExBitmask8_56_64)
using EPCGExBitmask8_56_64Bitmask = TEnumAsByte<EPCGExBitmask8_56_64>;

#pragma endregion

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FClampedBit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "63"))
	uint8 BitIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bValue;

	FClampedBit() : BitIndex(0), bValue(false)
	{
	}

	int64 Get() const { return bValue ? (static_cast<int64>(0) | (1LL << BitIndex)) : (static_cast<int64>(0) & (~(1LL << BitIndex))); }

	bool operator==(const FClampedBit& Other) const { return BitIndex == Other.BitIndex; }
	friend uint32 GetTypeHash(const FClampedBit& Key) { return HashCombine(0, GetTypeHash(Key.BitIndex)); }
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FClampedBitOp
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "63"))
	uint8 BitIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPCGExBitOp Op = EPCGExBitOp::OR;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bValue;

	FClampedBitOp() : BitIndex(0), bValue(true)
	{
	}

	int64 Get() const { return bValue ? (static_cast<int64>(0) | (1LL << BitIndex)) : (static_cast<int64>(0) & (~(1LL << BitIndex))); }

	bool operator==(const FClampedBitOp& Other) const { return BitIndex == Other.BitIndex; }
	friend uint32 GetTypeHash(const FClampedBitOp& Key) { return HashCombine(0, GetTypeHash(Key.BitIndex)); }
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExBitmask
{
	GENERATED_BODY()

	FPCGExBitmask()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExBitmaskMode Mode = EPCGExBitmaskMode::Individual;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode==EPCGExBitmaskMode::Direct", EditConditionHides))
	int64 Bitmask = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode==EPCGExBitmaskMode::Individual", TitleProperty="Bit # {BitIndex} = {bValue}", EditConditionHides))
	TArray<FClampedBit> Bits;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_00_08", DisplayName="0-8 Bits", EditCondition="Mode==EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_00_08 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_08_16", DisplayName="8-16 Bits", EditCondition="Mode==EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_08_16 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_16_24", DisplayName="16-24 Bits", EditCondition="Mode==EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_16_24 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_24_32", DisplayName="24-32 Bits", EditCondition="Mode==EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_24_32 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_32_40", DisplayName="32-40 Bits", EditCondition="Mode==EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_32_40 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_40_48", DisplayName="40-48 Bits", EditCondition="Mode==EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_40_48 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_48_56", DisplayName="48-56 Bits", EditCondition="Mode==EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_48_56 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_56_64", DisplayName="56-64 Bits", EditCondition="Mode==EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_56_64 = 0;

	int64 Get() const
	{
		int64 Mask = 0;

		if (Mode == EPCGExBitmaskMode::Direct) { return Bitmask; }

		if (Mode == EPCGExBitmaskMode::Individual)
		{
			for (const FClampedBit& Bit : Bits) { if (Bit.bValue) { Mask |= (1LL << Bit.BitIndex); } }
		}
		else
		{
			Mask |= static_cast<int64>(Range_00_08) << 0;
			Mask |= static_cast<int64>(Range_08_16) << 8;
			Mask |= static_cast<int64>(Range_16_24) << 16;
			Mask |= static_cast<int64>(Range_24_32) << 24;
			Mask |= static_cast<int64>(Range_32_40) << 32;
			Mask |= static_cast<int64>(Range_40_48) << 40;
			Mask |= static_cast<int64>(Range_48_56) << 48;
			Mask |= static_cast<int64>(Range_56_64) << 56;
		}

		return Mask;
	}

	void DoOperation(EPCGExBitOp Op, int64& Flags) const
	{
		const int64 Mask = Get();
		switch (Op)
		{
		case EPCGExBitOp::Set:
			Flags = Mask;
			break;
		case EPCGExBitOp::AND:
			Flags &= Mask;
			break;
		case EPCGExBitOp::OR:
			Flags |= Mask;
			break;
		case EPCGExBitOp::NOT:
			Flags &= ~Mask;
			break;
		case EPCGExBitOp::XOR:
			Flags ^= Mask;
			break;
		default: ;
		}
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExBitmaskWithOperation
{
	GENERATED_BODY()

	FPCGExBitmaskWithOperation()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExBitmaskMode Mode = EPCGExBitmaskMode::Individual;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode==EPCGExBitmaskMode::Direct", EditConditionHides))
	int64 Bitmask = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode==EPCGExBitmaskMode::Individual", TitleProperty="Bit # {BitIndex} = {bValue}", EditConditionHides))
	TArray<FClampedBitOp> Bits;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(PCG_NotOverridable, EditCondition="Mode!=EPCGExBitmaskMode::Individual", EditConditionHides))
	EPCGExBitOp Op = EPCGExBitOp::OR;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_00_08", DisplayName="0-8 Bits", EditCondition="Mode==EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_00_08 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_08_16", DisplayName="8-16 Bits", EditCondition="Mode==EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_08_16 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_16_24", DisplayName="16-24 Bits", EditCondition="Mode==EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_16_24 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_24_32", DisplayName="24-32 Bits", EditCondition="Mode==EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_24_32 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_32_40", DisplayName="32-40 Bits", EditCondition="Mode==EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_32_40 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_40_48", DisplayName="40-48 Bits", EditCondition="Mode==EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_40_48 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_48_56", DisplayName="48-56 Bits", EditCondition="Mode==EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_48_56 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_56_64", DisplayName="56-64 Bits", EditCondition="Mode==EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_56_64 = 0;

	int64 Get() const
	{
		int64 Mask = 0;

		switch (Mode)
		{
		case EPCGExBitmaskMode::Direct:
			Mask = Bitmask;
			break;
		case EPCGExBitmaskMode::Individual:
			for (const FClampedBitOp& Bit : Bits) { if (Bit.bValue) { Mask |= (1LL << Bit.BitIndex); } }
			break;
		case EPCGExBitmaskMode::Composite:
			Mask |= static_cast<int64>(Range_00_08) << 0;
			Mask |= static_cast<int64>(Range_08_16) << 8;
			Mask |= static_cast<int64>(Range_16_24) << 16;
			Mask |= static_cast<int64>(Range_24_32) << 24;
			Mask |= static_cast<int64>(Range_32_40) << 32;
			Mask |= static_cast<int64>(Range_40_48) << 40;
			Mask |= static_cast<int64>(Range_48_56) << 48;
			Mask |= static_cast<int64>(Range_56_64) << 56;
			break;
		default: ;
		}

		return Mask;
	}

	void DoOperation(int64& Flags) const
	{
		if (Mode == EPCGExBitmaskMode::Individual)
		{
			for (const FClampedBitOp& BitOp : Bits)
			{
				int64 Bit = BitOp.Get();
				switch (BitOp.Op)
				{
				case EPCGExBitOp::Set:
					if (BitOp.bValue) { Flags |= Bit; } // Set the bit
					else { Flags &= Bit; }              // Clear the bit
					break;
				case EPCGExBitOp::AND:
					Flags &= Bit;
					break;
				case EPCGExBitOp::OR:
					Flags |= Bit;
					break;
				case EPCGExBitOp::NOT:
					Flags &= ~Bit;
					break;
				case EPCGExBitOp::XOR:
					Flags ^= Bit;
					break;
				default: ;
				}
			}
			return;
		}

		const int64 Mask = Get();

		switch (Op)
		{
		case EPCGExBitOp::Set:
			Flags = Mask;
			break;
		case EPCGExBitOp::AND:
			Flags &= Mask;
			break;
		case EPCGExBitOp::OR:
			Flags |= Mask;
			break;
		case EPCGExBitOp::NOT:
			Flags &= ~Mask;
			break;
		case EPCGExBitOp::XOR:
			Flags ^= Mask;
			break;
		default: ;
		}
	}
};

#undef PCGEX_UNSUPPORTED_STRING_TYPES
#undef PCGEX_UNSUPPORTED_PATH_TYPES
