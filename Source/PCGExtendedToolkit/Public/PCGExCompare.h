// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "Metadata/PCGAttributePropertySelector.h"

#include "PCGExCompare.generated.h"

#define PCGEX_UNSUPPORTED_STRING_TYPES(MACRO)\
MACRO(FString)\
MACRO(FName)\
MACRO(FSoftObjectPath)\
MACRO(FSoftClassPath)

#define PCGEX_UNSUPPORTED_PATH_TYPES(MACRO)\
MACRO(FSoftObjectPath)\
MACRO(FSoftClassPath)

struct FPCGExContext;

namespace PCGExData
{
	class FFacade;
	class FTags;
	class IDataValue;
	class FFacadePreloader;
}

namespace PCGExDetails
{
	template<typename T>
	class TSettingValue;
}

UENUM()
enum class EPCGExIndexMode : uint8
{
	Pick   = 0 UMETA(DisplayName = "Pick", ToolTip="Index value represent a specific pick"),
	Offset = 1 UMETA(DisplayName = "Offset", ToolTip="Index value represent an offset from current point' index"),
};

UENUM()
enum class EPCGExAngularDomain : uint8
{
	Scalar  = 0 UMETA(DisplayName = "Scalar", Tooltip="Read the value as the result of a normalized dot product"),
	Degrees = 1 UMETA(DisplayName = "Degrees", Tooltip="Read the value as degrees"),
};

UENUM(BlueprintType)
enum class EPCGExComparison : uint8
{
	StrictlyEqual    = 0 UMETA(DisplayName = " == ", Tooltip="Operand A Strictly Equal to Operand B"),
	StrictlyNotEqual = 1 UMETA(DisplayName = " != ", Tooltip="Operand A Strictly Not Equal to Operand B"),
	EqualOrGreater   = 2 UMETA(DisplayName = " >= ", Tooltip="Operand A Equal or Greater to Operand B"),
	EqualOrSmaller   = 3 UMETA(DisplayName = " <= ", Tooltip="Operand A Equal or Smaller to Operand B"),
	StrictlyGreater  = 4 UMETA(DisplayName = " > ", Tooltip="Operand A Strictly Greater to Operand B"),
	StrictlySmaller  = 5 UMETA(DisplayName = " < ", Tooltip="Operand A Strictly Smaller to Operand B"),
	NearlyEqual      = 6 UMETA(DisplayName = " ~= ", Tooltip="Operand A Nearly Equal to Operand B"),
	NearlyNotEqual   = 7 UMETA(DisplayName = " !~= ", Tooltip="Operand A Nearly Not Equal to Operand B"),
};

UENUM()
enum class EPCGExEquality : uint8
{
	Equal    = 0 UMETA(DisplayName = " == ", Tooltip="Operand A Equal to Operand B"),
	NotEqual = 1 UMETA(DisplayName = " != ", Tooltip="Operand A Not Equal to Operand B"),
};

UENUM(BlueprintType)
enum class EPCGExStringComparison : uint8
{
	StrictlyEqual         = 0 UMETA(DisplayName = " == ", Tooltip="Operand A Strictly Equal to Operand B"),
	StrictlyNotEqual      = 1 UMETA(DisplayName = " != ", Tooltip="Operand A Strictly Not Equal to Operand B"),
	LengthStrictlyEqual   = 2 UMETA(DisplayName = " == (Length) ", Tooltip="Operand A Strictly Equal to Operand B"),
	LengthStrictlyUnequal = 3 UMETA(DisplayName = " != (Length) ", Tooltip="Operand A Strictly Not Equal to Operand B"),
	LengthEqualOrGreater  = 4 UMETA(DisplayName = " >= (Length)", Tooltip="Operand A Equal or Greater to Operand B"),
	LengthEqualOrSmaller  = 5 UMETA(DisplayName = " <= (Length)", Tooltip="Operand A Equal or Smaller to Operand B"),
	StrictlyGreater       = 6 UMETA(DisplayName = " > (Length)", Tooltip="Operand A Strictly Greater to Operand B"),
	StrictlySmaller       = 7 UMETA(DisplayName = " < (Length)", Tooltip="Operand A Strictly Smaller to Operand B"),
	LocaleStrictlyGreater = 8 UMETA(DisplayName = " > (Locale)", Tooltip="Operand A Locale Strictly Greater to Operand B Locale"),
	LocaleStrictlySmaller = 9 UMETA(DisplayName = " < (Locale)", Tooltip="Operand A Locale Strictly Smaller to Operand B Locale"),
	Contains              = 10 UMETA(DisplayName = " Contains ", Tooltip="Operand A contains Operand B"),
	StartsWith            = 11 UMETA(DisplayName = " Starts With ", Tooltip="Operand A starts with Operand B"),
	EndsWith              = 12 UMETA(DisplayName = " Ends With ", Tooltip="Operand A ends with Operand B"),
};

UENUM()
enum class EPCGExStringMatchMode : uint8
{
	Equals     = 0 UMETA(DisplayName = "Equals", ToolTip=""),
	Contains   = 1 UMETA(DisplayName = "Contains", ToolTip=""),
	StartsWith = 2 UMETA(DisplayName = "Starts with", ToolTip=""),
	EndsWith   = 3 UMETA(DisplayName = "Ends with", ToolTip=""),
};

UENUM()
enum class EPCGExBitflagComparison : uint8
{
	MatchPartial   = 0 UMETA(DisplayName = "Match (any)", Tooltip="Value & Mask != 0 (At least some flags in the mask are set)"),
	MatchFull      = 1 UMETA(DisplayName = "Match (all)", Tooltip="Value & Mask == Mask (All the flags in the mask are set)"),
	MatchStrict    = 2 UMETA(DisplayName = "Match (strict)", Tooltip="Value == Mask (Flags strictly equals mask)"),
	NoMatchPartial = 3 UMETA(DisplayName = "No match (any)", Tooltip="Value & Mask == 0 (Flags does not contains any from mask)"),
	NoMatchFull    = 4 UMETA(DisplayName = "No match (all)", Tooltip="Value & Mask != Mask (Flags does not contains the mask)"),
};

UENUM()
enum class EPCGExComparisonDataType : uint8
{
	Numeric = 0 UMETA(DisplayName = "Numeric", Tooltip="Compare numeric values"),
	String  = 1 UMETA(DisplayName = "String", Tooltip="Compare string values"),
};

namespace PCGExCompare
{
	FString ToString(const EPCGExComparison Comparison);
	FString ToString(const EPCGExBitflagComparison Comparison);
	FString ToString(const EPCGExStringComparison Comparison);
	FString ToString(const EPCGExStringMatchMode MatchMode);

	bool Compare(const EPCGExStringComparison Method, const FString& A, const FString& B);

#pragma region Numeric comparisons ops

	template <typename T>
	FORCEINLINE static bool StrictlyEqual(const T& A, const T& B) { return A == B; }

	template <typename T>
	FORCEINLINE static bool StrictlyNotEqual(const T& A, const T& B) { return A != B; }

	template <typename T>
	FORCEINLINE static bool EqualOrGreater(const T& A, const T& B)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return A == B || A;
		}
		else if constexpr (
			std::is_same_v<T, FVector2D> ||
			std::is_same_v<T, FVector>)
		{
			return A.SquaredLength() >= B.SquaredLength();
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			return EqualOrGreater(FVector(A), FVector(B));
		}
		else if constexpr (
			std::is_same_v<T, FQuat> ||
			std::is_same_v<T, FRotator>)
		{
			return EqualOrGreater(FVector(A.Euler()), FVector(B.Euler()));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return (
				EqualOrGreater(A.GetLocation(), B.GetLocation()) &&
				EqualOrGreater(A.GetRotation(), B.GetRotation()) &&
				EqualOrGreater(A.GetScale3D(), B.GetScale3D()));
		}
		else if constexpr (std::is_same_v<T, FString>)
		{
			return A.Compare(B, ESearchCase::IgnoreCase) >= 0;
		}
		else if constexpr (
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftObjectPath> ||
			std::is_same_v<T, FSoftClassPath>)
		{
			return EqualOrGreater(A.ToString(), B.ToString());
		}
		else
		{
			return A >= B;
		}
	}

	template <typename T>
	FORCEINLINE static bool EqualOrSmaller(const T& A, const T& B)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return A == B || !A;
		}
		else if constexpr (
			std::is_same_v<T, FVector2D> ||
			std::is_same_v<T, FVector>)
		{
			return A.SquaredLength() <= B.SquaredLength();
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			return EqualOrSmaller(FVector(A), FVector(B));
		}
		else if constexpr (
			std::is_same_v<T, FQuat> ||
			std::is_same_v<T, FRotator>)
		{
			return EqualOrSmaller(FVector(A.Euler()), FVector(B.Euler()));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return (
				EqualOrSmaller(A.GetLocation(), B.GetLocation()) &&
				EqualOrSmaller(A.GetRotation(), B.GetRotation()) &&
				EqualOrSmaller(A.GetScale3D(), B.GetScale3D()));
		}
		else if constexpr (std::is_same_v<T, FString>)
		{
			return A.Compare(B, ESearchCase::IgnoreCase) <= 0;
		}
		else if constexpr (
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftObjectPath> ||
			std::is_same_v<T, FSoftClassPath>)
		{
			return EqualOrSmaller(A.ToString(), B.ToString());
		}
		else
		{
			return A <= B;
		}
	}

	template <typename T>
	FORCEINLINE static bool StrictlyGreater(const T& A, const T& B)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return A && !B;
		}
		else if constexpr (
			std::is_same_v<T, FVector2D> ||
			std::is_same_v<T, FVector>)
		{
			return A.SquaredLength() > B.SquaredLength();
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			return StrictlyGreater(FVector(A), FVector(B));
		}
		else if constexpr (
			std::is_same_v<T, FQuat> ||
			std::is_same_v<T, FRotator>)
		{
			return StrictlyGreater(FVector(A.Euler()), FVector(B.Euler()));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return (
				StrictlyGreater(A.GetLocation(), B.GetLocation()) &&
				StrictlyGreater(A.GetRotation(), B.GetRotation()) &&
				StrictlyGreater(A.GetScale3D(), B.GetScale3D()));
		}
		else if constexpr (std::is_same_v<T, FString>)
		{
			return A.Compare(B, ESearchCase::IgnoreCase) > 0;
		}
		else if constexpr (
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftObjectPath> ||
			std::is_same_v<T, FSoftClassPath>)
		{
			return StrictlyGreater(A.ToString(), B.ToString());
		}
		else
		{
			return A > B;
		}
	}

	template <typename T>
	FORCEINLINE static bool StrictlySmaller(const T& A, const T& B)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return !A && B;
		}
		else if constexpr (
			std::is_same_v<T, FVector2D> ||
			std::is_same_v<T, FVector>)
		{
			return A.SquaredLength() < B.SquaredLength();
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			return StrictlySmaller(FVector(A), FVector(B));
		}
		else if constexpr (
			std::is_same_v<T, FQuat> ||
			std::is_same_v<T, FRotator>)
		{
			return StrictlySmaller(FVector(A.Euler()), FVector(B.Euler()));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return (
				StrictlySmaller(A.GetLocation(), B.GetLocation()) &&
				StrictlySmaller(A.GetRotation(), B.GetRotation()) &&
				StrictlySmaller(A.GetScale3D(), B.GetScale3D()));
		}
		else if constexpr (std::is_same_v<T, FString>)
		{
			return A.Compare(B, ESearchCase::IgnoreCase) < 0;
		}
		else if constexpr (
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftObjectPath> ||
			std::is_same_v<T, FSoftClassPath>)
		{
			return StrictlySmaller(A.ToString(), B.ToString());
		}
		else
		{
			return A < B;
		}
	}

	template <typename T>
	FORCEINLINE static bool NearlyEqual(const T& A, const T& B, const double Tolerance = DBL_COMPARE_TOLERANCE)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return A == B;
		}
		else if constexpr (std::is_same_v<T, FVector2D>)
		{
			return
				NearlyEqual(A.X, B.X, Tolerance) &&
				NearlyEqual(A.Y, B.Y, Tolerance);
		}
		else if constexpr (std::is_same_v<T, FVector2D>)
		{
			return
				NearlyEqual(A.X, B.X, Tolerance) &&
				NearlyEqual(A.Y, B.Y, Tolerance) &&
				NearlyEqual(A.Z, B.Z, Tolerance);
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			return
				NearlyEqual(A.X, B.X, Tolerance) &&
				NearlyEqual(A.Y, B.Y, Tolerance) &&
				NearlyEqual(A.Z, B.Z, Tolerance) &&
				NearlyEqual(A.W, B.W, Tolerance);
		}
		else if constexpr (
			std::is_same_v<T, FQuat> ||
			std::is_same_v<T, FRotator>)
		{
			return NearlyEqual(FVector(A.Euler()), FVector(B.Euler()), Tolerance);
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return
				NearlyEqual(A.GetLocation(), B.GetLocation(), Tolerance) &&
				NearlyEqual(A.GetRotation(), B.GetRotation(), Tolerance) &&
				NearlyEqual(A.GetScale3D(), B.GetScale3D(), Tolerance);
		}
		else if constexpr (std::is_same_v<T, FString>)
		{
			return FMath::IsNearlyEqual(A.Len(), B.Len(), Tolerance);
		}
		else if constexpr (
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftObjectPath> ||
			std::is_same_v<T, FSoftClassPath>)
		{
			return NearlyEqual(A.ToString(), B.ToString(), Tolerance);
		}
		else
		{
			return FMath::IsNearlyEqual(A, B, Tolerance);
		}
	}

	template <typename T>
	FORCEINLINE static bool NearlyNotEqual(const T& A, const T& B, const double Tolerance = DBL_COMPARE_TOLERANCE) { return !NearlyEqual(A, B, Tolerance); }

#pragma endregion

	template <typename T>
	FORCEINLINE static bool Compare(const EPCGExComparison Method, const T& A, const T& B, const double Tolerance = DBL_COMPARE_TOLERANCE)
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

	bool Compare(const EPCGExComparison Method, const TSharedPtr<PCGExData::IDataValue>& A, const double B, const double Tolerance = DBL_COMPARE_TOLERANCE);
	bool Compare(const EPCGExStringComparison Method, const TSharedPtr<PCGExData::IDataValue>& A, const FString B);
	bool Compare(const EPCGExBitflagComparison Method, const int64& Flags, const int64& Mask);

	bool HasMatchingTags(const TSharedPtr<PCGExData::FTags>& InTags, const FString& Query, const EPCGExStringMatchMode MatchMode, const bool bStrict = true);
	bool GetMatchingValueTags(const TSharedPtr<PCGExData::FTags>& InTags, const FString& Query, const EPCGExStringMatchMode MatchMode, TArray<TSharedPtr<PCGExData::IDataValue>>& OutValues);
}

UENUM()
enum class EPCGExDirectionCheckMode : uint8
{
	Dot  = 0 UMETA(DisplayName = "Dot (Precise)", Tooltip="Extensive comparison using Dot product"),
	Hash = 1 UMETA(DisplayName = "Hash (Fast)", Tooltip="Simplified check using hash comparison with a destructive tolerance"),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExVectorHashComparisonDetails
{
	GENERATED_BODY()

	FPCGExVectorHashComparisonDetails()
	{
	}

	explicit FPCGExVectorHashComparisonDetails(double InHashToleranceConstant)
	{
		HashToleranceConstant = InHashToleranceConstant;
	}

	/** Type of Tolerance value source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType HashToleranceInput = EPCGExInputValueType::Constant;

	/** Tolerance value use for comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Hash Tolerance (Attr)", EditCondition="HashToleranceInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector HashToleranceAttribute;

	/** Tolerance value use for comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Hash Tolerance", EditCondition="HashToleranceInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0.00001))
	double HashToleranceConstant = 0.001;

	TSharedPtr<PCGExDetails::TSettingValue<double>> Tolerance;

	TSharedPtr<PCGExDetails::TSettingValue<double>> GetValueSettingTolerance(const bool bQuietErrors = false) const;

	bool Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InPrimaryDataFacade);
	FVector GetCWTolerance(const int32 PointIndex) const;

	void RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const;
	bool GetOnlyUseDataDomain() const;

	bool Test(const FVector& A, const FVector& B, const int32 PointIndex) const;
};

/**
 * Util object to encapsulate recurring dot comparison parameters, with no support for params
 */
USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExStaticDotComparisonDetails
{
	GENERATED_BODY()

	FPCGExStaticDotComparisonDetails()
	{
	}


	/** Value domain (units) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExAngularDomain Domain = EPCGExAngularDomain::Scalar;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExComparison Comparison = EPCGExComparison::EqualOrGreater;

	/** If enabled, the dot product will be made absolute before testing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bUnsignedComparison = false;

	/** Dot value use for comparison (In raw -1/1 range) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Scalar", EditCondition="Domain == EPCGExAngularDomain::Scalar", EditConditionHides, ClampMin=-1, ClampMax=1))
	double DotConstant = 0.5;

	/** Tolerance for dot comparison. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Tolerance", EditCondition="(Comparison == EPCGExComparison::NearlyEqual || Comparison == EPCGExComparison::NearlyNotEqual) && Domain == EPCGExAngularDomain::Scalar", EditConditionHides, ClampMin=0, ClampMax=1))
	double DotTolerance = 0.1;

	/** Dot value use for comparison (In degrees) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Degrees", EditCondition="Domain == EPCGExAngularDomain::Degrees", EditConditionHides, ClampMin=0, ClampMax=180, Units="Degrees"))
	double DegreesConstant = 90;

	/** Tolerance for dot comparison. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Tolerance", EditCondition="(Comparison == EPCGExComparison::NearlyEqual || Comparison == EPCGExComparison::NearlyNotEqual) && Domain == EPCGExAngularDomain::Degrees", EditConditionHides, ClampMin=0, ClampMax=180, Units="Degrees"))
	double DegreesTolerance = 0.1;

	double ComparisonTolerance = 0;

	void Init();
	bool Test(const double A) const;
};

/**
 * Util object to encapsulate recurring dot comparison parameters, including attribute-driven params
 */
USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExDotComparisonDetails
{
	GENERATED_BODY()

	FPCGExDotComparisonDetails()
	{
	}

	/** Value domain (units) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExAngularDomain Domain = EPCGExAngularDomain::Scalar;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExComparison Comparison = EPCGExComparison::EqualOrGreater;

	/** If enabled, the dot product will be made absolute before testing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bUnsignedComparison = false;

	/** Type of Dot value source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName="Threshold Input"))
	EPCGExInputValueType ThresholdInput = EPCGExInputValueType::Constant;

	/** Attribute value use for comparison, whether Scalar or Degrees */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Threshold (Attr)", EditCondition="ThresholdInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector ThresholdAttribute;

	/** Dot value use for comparison (In raw -1/1 range) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Scalar", EditCondition="ThresholdInput == EPCGExInputValueType::Constant && Domain == EPCGExAngularDomain::Scalar", EditConditionHides, ClampMin=-1, ClampMax=1))
	double DotConstant = 0.5;

	/** Tolerance for dot comparison. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Tolerance", EditCondition="(Comparison == EPCGExComparison::NearlyEqual || Comparison == EPCGExComparison::NearlyNotEqual) && Domain == EPCGExAngularDomain::Scalar", EditConditionHides, ClampMin=0, ClampMax=1))
	double DotTolerance = 0.1;

	/** Dot value use for comparison (In degrees) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Degrees", EditCondition="ThresholdInput == EPCGExInputValueType::Constant && Domain == EPCGExAngularDomain::Degrees", EditConditionHides, ClampMin=0, ClampMax=180, Units="Degrees"))
	double DegreesConstant = 90;

	/** Tolerance for dot comparison. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Tolerance", EditCondition="(Comparison == EPCGExComparison::NearlyEqual || Comparison == EPCGExComparison::NearlyNotEqual) && Domain == EPCGExAngularDomain::Degrees", EditConditionHides, ClampMin=0, ClampMax=180, Units="Degrees"))
	double DegreesTolerance = 0.1;

	TSharedPtr<PCGExDetails::TSettingValue<double>> GetValueSettingThreshold(const bool bQuietErrors = false) const;

	TSharedPtr<PCGExDetails::TSettingValue<double>> ThresholdGetter;

	double ComparisonTolerance = 0;

	bool Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InPrimaryDataCache);

	double GetComparisonThreshold(const int32 PointIndex) const;

	bool Test(const double A, const double B) const;
	bool Test(const double A, const int32 Index) const;

	void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const;
	void RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const;
	bool GetOnlyUseDataDomain() const;

#if WITH_EDITOR
	FString GetDisplayComparison() const;
#endif
};

UENUM()
enum class EPCGExBitOp : uint8
{
	Set = 0 UMETA(DisplayName = "=", ToolTip="(Flags = Mask) Set the bit with the specified value."),
	AND = 1 UMETA(DisplayName = "AND", ToolTip="(Flags &= Mask) Output true if boths bits == 1, otherwise false."),
	OR  = 2 UMETA(DisplayName = "OR", ToolTip="(Flags |= Mask) Output true if any of the bits == 1, otherwise false."),
	NOT = 3 UMETA(DisplayName = "NOT", ToolTip="(Flags &= ~Mask) Like AND, but inverts the masks."),
	XOR = 4 UMETA(DisplayName = "XOR", ToolTip="(Flags ^= Mask) Invert the flag bit where the mask == 1."),
};

UENUM()
enum class EPCGExBitmaskMode : uint8
{
	Direct     = 0 UMETA(DisplayName = "Direct", ToolTip="Used for easy override mostly"),
	Individual = 1 UMETA(DisplayName = "Individual", ToolTip="Use an array to manually set the bits"),
	Composite  = 2 UMETA(DisplayName = "Composite", ToolTip="Use a ton of dropdown to set the bits"),
};

#pragma region Bitmasks

UENUM(meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 0-8 Bits Range"))
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

UENUM(meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 8-16 Bits Range"))
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

UENUM(meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 16-24 Bits Range"))
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

UENUM(meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 24-32 Bits Range"))
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

UENUM(meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 32-40 Bits Range"))
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

UENUM(meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 40-48 Bits Range"))
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

UENUM(meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 48-56 Bits Range"))
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

UENUM(meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Bitflag 56-64 Bits Range"))
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (ClampMin = "0", ClampMax = "63"))
	uint8 BitIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (ClampMin = "0", ClampMax = "63"))
	uint8 BitIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	EPCGExBitOp Op = EPCGExBitOp::OR;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode == EPCGExBitmaskMode::Direct", EditConditionHides))
	int64 Bitmask = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode == EPCGExBitmaskMode::Individual", TitleProperty="Bit # {BitIndex} = {bValue}", EditConditionHides))
	TArray<FClampedBit> Bits;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_00_08", DisplayName="0-8 Bits", EditCondition="Mode == EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_00_08 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_08_16", DisplayName="8-16 Bits", EditCondition="Mode == EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_08_16 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_16_24", DisplayName="16-24 Bits", EditCondition="Mode == EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_16_24 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_24_32", DisplayName="24-32 Bits", EditCondition="Mode == EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_24_32 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_32_40", DisplayName="32-40 Bits", EditCondition="Mode == EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_32_40 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_40_48", DisplayName="40-48 Bits", EditCondition="Mode == EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_40_48 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_48_56", DisplayName="48-56 Bits", EditCondition="Mode == EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_48_56 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_56_64", DisplayName="56-64 Bits", EditCondition="Mode == EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_56_64 = 0;

	int64 Get() const;
	void DoOperation(const EPCGExBitOp Op, int64& Flags) const;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExBitmaskWithOperation
{
	GENERATED_BODY()

	FPCGExBitmaskWithOperation()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExBitmaskMode Mode = EPCGExBitmaskMode::Direct;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode == EPCGExBitmaskMode::Direct", EditConditionHides))
	int64 Bitmask = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode == EPCGExBitmaskMode::Individual", TitleProperty="Bit # {BitIndex} = {bValue}", EditConditionHides))
	TArray<FClampedBitOp> Bits;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode != EPCGExBitmaskMode::Individual", EditConditionHides))
	EPCGExBitOp Op = EPCGExBitOp::OR;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_00_08", DisplayName="0-8 Bits", EditCondition="Mode == EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_00_08 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_08_16", DisplayName="8-16 Bits", EditCondition="Mode == EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_08_16 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_16_24", DisplayName="16-24 Bits", EditCondition="Mode == EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_16_24 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_24_32", DisplayName="24-32 Bits", EditCondition="Mode == EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_24_32 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_32_40", DisplayName="32-40 Bits", EditCondition="Mode == EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_32_40 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_40_48", DisplayName="40-48 Bits", EditCondition="Mode == EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_40_48 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_48_56", DisplayName="48-56 Bits", EditCondition="Mode == EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_48_56 = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitmask8_56_64", DisplayName="56-64 Bits", EditCondition="Mode == EPCGExBitmaskMode::Composite", EditConditionHides))
	uint8 Range_56_64 = 0;

	int64 Get() const;
	void DoOperation(int64& Flags) const;
};

#undef PCGEX_UNSUPPORTED_STRING_TYPES
#undef PCGEX_UNSUPPORTED_PATH_TYPES
