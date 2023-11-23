// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "PCGComponent.h"

#include "PCGExCommon.generated.h"

//virtual FName GetDefaultNodeName() const override { return FName(TEXT(#_SHORTNAME)); } \
//virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGEx" #_SHORTNAME, "NodeTitle", "PCGEx | " _NAME); }

#define PCGEX_NODE_INFOS(_SHORTNAME, _NAME, _TOOLTIP)\
virtual FName GetDefaultNodeName() const override { return FName(TEXT(#_SHORTNAME)); } \
virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGEx" #_SHORTNAME, "NodeTitle", "PCGEx | " _NAME);} \
virtual FText GetNodeTooltipText() const override{ return NSLOCTEXT("PCGEx" #_SHORTNAME "Tooltip", "NodeTooltip", _TOOLTIP); }

#define PCGEX_FOREACH_SUPPORTEDTYPES(MACRO) \
MACRO(bool, Boolean)       \
MACRO(int32, Integer32)      \
MACRO(int64, Integer64)      \
MACRO(float, Float)      \
MACRO(double, Double)     \
MACRO(FVector2D, Vector2D)  \
MACRO(FVector, Vector)    \
MACRO(FVector4, Vector4)   \
MACRO(FQuat, Quaternion)      \
MACRO(FRotator, Rotator)   \
MACRO(FTransform, Transform) \
MACRO(FString, String)    \
MACRO(FName, Name)

#define PCGEX_FOREACH_POINTPROPERTY(MACRO)\
MACRO(EPCGPointProperties::Density, Density) \
MACRO(EPCGPointProperties::BoundsMin, BoundsMin) \
MACRO(EPCGPointProperties::BoundsMax, BoundsMax) \
MACRO(EPCGPointProperties::Extents, GetExtents()) \
MACRO(EPCGPointProperties::Color, Color) \
MACRO(EPCGPointProperties::Position, Transform.GetLocation()) \
MACRO(EPCGPointProperties::Rotation, Transform.Rotator()) \
MACRO(EPCGPointProperties::Scale, Transform.GetScale3D()) \
MACRO(EPCGPointProperties::Transform, Transform) \
MACRO(EPCGPointProperties::Steepness, Steepness) \
MACRO(EPCGPointProperties::LocalCenter, GetLocalCenter()) \
MACRO(EPCGPointProperties::Seed, Seed)
#define PCGEX_FOREACH_POINTEXTRAPROPERTY(MACRO)\
MACRO(EPCGExtraProperties::Index, MetadataEntry)

class FAssetRegistryModule;

UENUM(BlueprintType)
enum class EPCGExOrderedFieldSelection : uint8
{
	X UMETA(DisplayName = "X", ToolTip="X/Roll component if it exist, raw value otherwise."),
	Y UMETA(DisplayName = "Y (→x)", ToolTip="Y/Pitch component if it exist, fallback to previous value otherwise."),
	Z UMETA(DisplayName = "Z (→y)", ToolTip="Z/Yaw component if it exist, fallback to previous value otherwise."),
	W UMETA(DisplayName = "W (→z)", ToolTip="W component if it exist, fallback to previous value otherwise."),
	XYZ UMETA(DisplayName = "X→Y→Z", ToolTip="X, then Y, then Z. Mostly for comparisons, fallback to X/Roll otherwise"),
	XZY UMETA(DisplayName = "X→Z→Y", ToolTip="X, then Z, then Y. Mostly for comparisons, fallback to X/Roll otherwise"),
	YXZ UMETA(DisplayName = "Y→X→Z", ToolTip="Y, then X, then Z. Mostly for comparisons, fallback to Y/Pitch otherwise"),
	YZX UMETA(DisplayName = "Y→Z→X", ToolTip="Y, then Z, then X. Mostly for comparisons, fallback to Y/Pitch otherwise"),
	ZXY UMETA(DisplayName = "Z→X→Y", ToolTip="Z, then X, then Y. Mostly for comparisons, fallback to Z/Yaw otherwise"),
	ZYX UMETA(DisplayName = "Z→Y→X", ToolTip="Z, then Y, then Z. Mostly for comparisons, fallback to Z/Yaw otherwise"),
	Length UMETA(DisplayName = "Length", ToolTip="Length if vector, raw value otherwise."),
};

UENUM(BlueprintType)
enum class EPCGExSingleField : uint8
{
	X UMETA(DisplayName = "X/Roll", ToolTip="X/Roll component if it exist, raw value otherwise."),
	Y UMETA(DisplayName = "Y/Pitch", ToolTip="Y/Pitch component if it exist, fallback to previous value otherwise."),
	Z UMETA(DisplayName = "Z/Yaw", ToolTip="Z/Yaw component if it exist, fallback to previous value otherwise."),
	W UMETA(DisplayName = "W", ToolTip="W component if it exist, fallback to previous value otherwise."),
	Length UMETA(DisplayName = "Length", ToolTip="Length if vector, raw value otherwise."),
};

UENUM(BlueprintType)
enum class EPCGExAxis : uint8
{
	Forward UMETA(DisplayName = "Default (Forward)", ToolTip="Forward from Transform/FQuat/Rotator, or raw vector."),
	Backward UMETA(DisplayName = "Backward", ToolTip="Backward from Transform/FQuat/Rotator, or raw vector."),
	Right UMETA(DisplayName = "Right", ToolTip="Right from Transform/FQuat/Rotator, or raw vector."),
	Left UMETA(DisplayName = "Left", ToolTip="Left from Transform/FQuat/Rotator, or raw vector."),
	Up UMETA(DisplayName = "Up", ToolTip="Up from Transform/FQuat/Rotator, or raw vector."),
	Down UMETA(DisplayName = "Down", ToolTip="Down from Transform/FQuat/Rotator, or raw vector."),
};

UENUM(BlueprintType)
enum class EPCGExCollisionFilterType : uint8
{
	Channel UMETA(DisplayName = "Channel", ToolTip="TBD"),
	ObjectType UMETA(DisplayName = "Object Type", ToolTip="TBD"),
};

UENUM(BlueprintType)
enum class EPCGExSelectorType : uint8
{
	SingleField UMETA(DisplayName = "Single Field", ToolTip="Forward from Transform/FQuat/Rotator, or raw vector."),
	Direction UMETA(DisplayName = "Direction", ToolTip="Backward from Transform/FQuat/Rotator, or raw vector."),
};


#pragma region Input Descriptors

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputDescriptor
{
	GENERATED_BODY()

	FPCGExInputDescriptor()
	{
		bValidatedAtLeastOnce = false;
	}

	FPCGExInputDescriptor(const FPCGExInputDescriptor& Other): FPCGExInputDescriptor()
	{
		Selector = Other.Selector;
		Attribute = Other.Attribute;
	}

public:
	virtual ~FPCGExInputDescriptor()
	{
		Attribute = nullptr;
	};

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (HideInDetailPanel, Hidden, EditConditionHides, EditCondition="false"))
	FString HiddenDisplayName;

	/** Point Attribute or $Property */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayPriority=0))
	FPCGAttributePropertyInputSelector Selector;

	FPCGMetadataAttributeBase* Attribute = nullptr;
	bool bValidatedAtLeastOnce = false;
	int16 UnderlyingType = 0;

	/**
	 * 
	 * @tparam T 
	 * @return 
	 */
	template <typename T>
	FPCGMetadataAttribute<T>* GetTypedAttribute()
	{
		if (Attribute == nullptr) { return nullptr; }
		return static_cast<FPCGMetadataAttribute<T>*>(Attribute);
	}

public:
	EPCGAttributePropertySelection GetSelection() const { return Selector.GetSelection(); }
	FName GetName() const { return Selector.GetName(); }

	/**
	 * Validate & cache the current selector for a given UPCGPointData
	 * @param InData 
	 * @return 
	 */
	bool Validate(const UPCGPointData* InData)
	{
		bValidatedAtLeastOnce = true;
		Selector = Selector.CopyAndFixLast(InData);

		if (GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			Attribute = Selector.IsValid() ? InData->Metadata->GetMutableAttribute(GetName()) : nullptr;

			if (Attribute)
			{
				const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InData, Selector);
				UnderlyingType = Accessor->GetUnderlyingType();
				//if (!Accessor.IsValid()) { Attribute = nullptr; }
			}

			return Attribute != nullptr;
		}
		else if (Selector.IsValid())
		{
			const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InData, Selector);
			UnderlyingType = Accessor->GetUnderlyingType();
			return true;
		}
		return false;
	}

	FString ToString() const { return GetName().ToString(); }
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputDescriptorGeneric : public FPCGExInputDescriptor
{
	GENERATED_BODY()

	FPCGExInputDescriptorGeneric(): FPCGExInputDescriptor()
	{
	}

	FPCGExInputDescriptorGeneric(const FPCGExInputDescriptorGeneric& Other)
		: FPCGExInputDescriptor(Other),
		  Type(Other.Type),
		  Axis(Other.Axis),
		  Field(Other.Field)
	{
	}

public:
	/** How to interpret the data */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayAfter="Selector"))
	EPCGExSelectorType Type = EPCGExSelectorType::SingleField;

	/** Direction to sample on relevant data types (FQuat are transformed to a direction first, from which the single component is selected) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName=" └─ Axis", PCG_Overridable, DisplayAfter="Type"))
	EPCGExAxis Axis = EPCGExAxis::Forward;

	/** Single field selection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName=" └─ Field", PCG_Overridable, DisplayAfter="Axis", EditCondition="Type==EPCGExSelectorType::SingleField", EditConditionHides))
	EPCGExSingleField Field = EPCGExSingleField::X;

	~FPCGExInputDescriptorGeneric()
	{
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputDescriptorWithDirection : public FPCGExInputDescriptor
{
	GENERATED_BODY()

	FPCGExInputDescriptorWithDirection(): FPCGExInputDescriptor()
	{
	}

	FPCGExInputDescriptorWithDirection(const FPCGExInputDescriptorWithDirection& Other)
		: FPCGExInputDescriptor(Other),
		  Axis(Other.Axis)
	{
	}

public:
	/** Sub-component order, used only for multi-field attributes (FVector, FRotator etc). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName=" └─ Axis", PCG_Overridable, DisplayAfter="Selector"))
	EPCGExAxis Axis = EPCGExAxis::Forward;

	~FPCGExInputDescriptorWithDirection()
	{
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputDescriptorWithSingleField : public FPCGExInputDescriptor
{
	GENERATED_BODY()

	FPCGExInputDescriptorWithSingleField(): FPCGExInputDescriptor()
	{
	}

	FPCGExInputDescriptorWithSingleField(const FPCGExInputDescriptorWithSingleField& Other)
		: FPCGExInputDescriptor(Other),
		  Axis(Other.Axis),
		  Field(Other.Field)
	{
	}

public:
	/** Direction to sample on relevant data types (FQuat are transformed to a direction first, from which the single component is selected) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName=" └─ Axis", PCG_Overridable, DisplayAfter="Selector"))
	EPCGExAxis Axis = EPCGExAxis::Forward;

	/** Single field selection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName=" └─ Field", PCG_Overridable, DisplayAfter="Axis"))
	EPCGExSingleField Field = EPCGExSingleField::X;

	~FPCGExInputDescriptorWithSingleField()
	{
	}
};


#pragma endregion

#pragma region Output Descriptors

#pragma endregion

namespace PCGEx
{
	const FName SourcePointsLabel = TEXT("InPoints");
	const FName SourceTargetPointsLabel = TEXT("InTargetPoints");
	const FName OutputPointsLabel = TEXT("OutPoints");

	const FSoftObjectPath DefaultDotOverDistanceCurve = FSoftObjectPath(TEXT("/PCGExtendedToolkit/FC_PCGExGraphBalance_Default.FC_PCGExGraphBalance_Default"));
	const FSoftObjectPath WeightDistributionLinear = FSoftObjectPath(TEXT("/PCGExtendedToolkit/FC_PCGExWeightDistribution_Linear.FC_PCGExWeightDistribution_Linear'"));

	class Common
	{
	public:
		static UWorld* GetWorld(const FPCGContext* Context)
		{
			check(Context->SourceComponent.IsValid());
			return Context->SourceComponent->GetWorld();
		}

		template <typename T>
		static FPCGMetadataAttribute<T>* TryGetAttribute(UPCGSpatialData* InData, FName Name, bool bEnabled, T defaultValue = T{})
		{
			if (!bEnabled || !PCGEx::Common::IsValidName(Name)) { return nullptr; }
			return InData->Metadata->FindOrCreateAttribute<T>(Name, defaultValue);
		}

		/**
		 * 
		 * @tparam LoopBodyFunc 
		 * @param Context The context containing the AsyncState
		 * @param NumIterations The number of calls that will be done to the provided function, also an upper bound on the number of data generated.
		 * @param Initialize Called once when the processing starts
		 * @param LoopBody Signature: bool(int32 ReadIndex, int32 WriteIndex).
		 * @param ChunkSize Size of the chunks to cut the input data with
		 * @return 
		 */
		static bool ParallelForLoop
			(
			FPCGContext* Context,
			const int32 NumIterations,
			TFunction<void()>&& Initialize,
			TFunction<void(int32)>&& LoopBody,
			const int32 ChunkSize = 32)
		{
			auto InnerBodyLoop = [&LoopBody](int32 ReadIndex, int32 WriteIndex)
			{
				LoopBody(ReadIndex);
				return true;
			};
			return FPCGAsync::AsyncProcessingOneToOneEx(&(Context->AsyncState), NumIterations, Initialize, InnerBodyLoop, true, ChunkSize);
		}

		static bool IsValidName(const FName& Name)
		{
			return IsValidName(Name.ToString());
		}

		static double ConvertStringToDouble(const FString& StringToConvert)
		{
			const TCHAR* CharArray = *StringToConvert;
			const double Result = FCString::Atod(CharArray);
			return FMath::IsNaN(Result) ? 0 : Result;
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
			}
		}

		/**
		 * 
		 * @param Name 
		 * @return 
		 */
		static bool IsValidName(const FString& Name)
		{
			// A valid name is alphanumeric with some special characters allowed.
			static const FString AllowedSpecialCharacters = TEXT(" _-/");

			for (int32 i = 0; i < Name.Len(); ++i)
			{
				if (FChar::IsAlpha(Name[i]) || FChar::IsDigit(Name[i]))
				{
					continue;
				}

				bool bAllowedSpecialCharacterFound = false;

				for (int32 j = 0; j < AllowedSpecialCharacters.Len(); ++j)
				{
					if (Name[i] == AllowedSpecialCharacters[j])
					{
						bAllowedSpecialCharacterFound = true;
						break;
					}
				}

				if (!bAllowedSpecialCharacterFound)
				{
					return false;
				}
			}

			return true;
		}
	};

	class Maths
	{
	public:
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
	};
}
