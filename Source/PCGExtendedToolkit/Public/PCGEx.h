// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGComponent.h"
#include "PCGContext.h"
#include "MatchAndSet/PCGMatchAndSetWeighted.h"
#include "Metadata/PCGMetadataAttribute.h"

#include "PCGEx.generated.h"

#pragma region MACROS

#define FTEXT(_TEXT) FText::FromString(FString(_TEXT))
#define FSTRING(_TEXT) FString(_TEXT)

#define PCGEX_DELETE(_VALUE) if(_VALUE){ delete _VALUE; _VALUE = nullptr; }
#define PCGEX_DELETE_TARRAY(_VALUE) for(const auto* Item : _VALUE){ delete Item; } _VALUE.Empty();
#define PCGEX_DELETE_TMAP(_VALUE, _TYPE){TArray<_TYPE> Keys; _VALUE.GetKeys(Keys); for (const _TYPE Key : Keys) { delete *_VALUE.Find(Key); } _VALUE.Empty(); Keys.Empty(); }
#define PCGEX_CLEANUP(_VALUE) _VALUE.Cleanup();
#define PCGEX_TRIM(_VALUE) _VALUE.SetNum(_VALUE.Num());

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 3
#define PCGEX_FOREACH_SUPPORTEDTYPES(MACRO, ...) \
MACRO(bool, Boolean, __VA_ARGS__)       \
MACRO(int32, Integer32, __VA_ARGS__)      \
MACRO(int64, Integer64, __VA_ARGS__)      \
MACRO(float, Float, __VA_ARGS__)      \
MACRO(double, Double, __VA_ARGS__)     \
MACRO(FVector2D, Vector2, __VA_ARGS__)  \
MACRO(FVector, Vector, __VA_ARGS__)    \
MACRO(FVector4, Vector4, __VA_ARGS__)   \
MACRO(FQuat, Quaternion, __VA_ARGS__)      \
MACRO(FRotator, Rotator, __VA_ARGS__)   \
MACRO(FTransform, Transform, __VA_ARGS__) \
MACRO(FString, String, __VA_ARGS__)    \
MACRO(FName, Name, __VA_ARGS__)
#else
#define PCGEX_FOREACH_SUPPORTEDTYPES(MACRO, ...) \
MACRO(bool, Boolean, __VA_ARGS__)       \
MACRO(int32, Integer32, __VA_ARGS__)      \
MACRO(int64, Integer64, __VA_ARGS__)      \
MACRO(float, Float, __VA_ARGS__)      \
MACRO(double, Double, __VA_ARGS__)     \
MACRO(FVector2D, Vector2, __VA_ARGS__)  \
MACRO(FVector, Vector, __VA_ARGS__)    \
MACRO(FVector4, Vector4, __VA_ARGS__)   \
MACRO(FQuat, Quaternion, __VA_ARGS__)      \
MACRO(FRotator, Rotator, __VA_ARGS__)   \
MACRO(FTransform, Transform, __VA_ARGS__) \
MACRO(FString, String, __VA_ARGS__)    \
MACRO(FName, Name, __VA_ARGS__)\
MACRO(FSoftObjectPath, SoftObjectPath, __VA_ARGS__)\
MACRO(FSoftClassPath, SoftClassPath, __VA_ARGS__)
#endif

/**
 * Enum, Point.[Getter]
 * @param MACRO 
 */
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

#define PCGEX_FOREACH_POINTPROPERTY_LEAN(MACRO)\
MACRO(Density) \
MACRO(BoundsMin) \
MACRO(BoundsMax) \
MACRO(Color) \
MACRO(Position) \
MACRO(Rotation) \
MACRO(Scale) \
MACRO(Steepness) \
MACRO(Seed)

/**
 * Name
 * @param MACRO 
 */
#define PCGEX_FOREACH_GETSET_POINTPROPERTY(MACRO)\
MACRO(Density) \
MACRO(BoundsMin) \
MACRO(BoundsMax) \
MACRO(Color) \
MACRO(Transform) \
MACRO(Steepness) \
MACRO(Seed)

#define PCGEX_FOREACH_POINTEXTRAPROPERTY(MACRO)\
MACRO(EPCGExtraProperties::Index, MetadataEntry)

#pragma endregion

namespace PCGExData
{
	struct FPointIO;
}

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Ordered Field Selection"))
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

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Transform Component Selector"))
enum class EPCGExTransformComponent : uint8
{
	Position UMETA(DisplayName = "Position", ToolTip="Position component."),
	Rotation UMETA(DisplayName = "Rotation", ToolTip="Rotation component."),
	Scale UMETA(DisplayName = "Scale", ToolTip="Scale component."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Minimal Axis"))
enum class EPCGExMinimalAxis : uint8
{
	None UMETA(DisplayName = "None", ToolTip="None"),
	X UMETA(DisplayName = "X", ToolTip="X Axis"),
	Y UMETA(DisplayName = "Y", ToolTip="Y Axis"),
	Z UMETA(DisplayName = "Z", ToolTip="Z Axis"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Single Component Selector"))
enum class EPCGExSingleField : uint8
{
	X UMETA(DisplayName = "X/Roll", ToolTip="X/Roll component if it exist, raw value otherwise."),
	Y UMETA(DisplayName = "Y/Pitch", ToolTip="Y/Pitch component if it exist, fallback to previous value otherwise."),
	Z UMETA(DisplayName = "Z/Yaw", ToolTip="Z/Yaw component if it exist, fallback to previous value otherwise."),
	W UMETA(DisplayName = "W", ToolTip="W component if it exist, fallback to previous value otherwise."),
	Length UMETA(DisplayName = "Length", ToolTip="Length if vector, raw value otherwise."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Axis Selector"))
enum class EPCGExAxis : uint8
{
	Forward UMETA(DisplayName = "Default (Forward)", ToolTip="Forward from Transform/FQuat/Rotator, or raw vector."),
	Backward UMETA(DisplayName = "Backward", ToolTip="Backward from Transform/FQuat/Rotator, or raw vector."),
	Right UMETA(DisplayName = "Right", ToolTip="Right from Transform/FQuat/Rotator, or raw vector."),
	Left UMETA(DisplayName = "Left", ToolTip="Left from Transform/FQuat/Rotator, or raw vector."),
	Up UMETA(DisplayName = "Up", ToolTip="Up from Transform/FQuat/Rotator, or raw vector."),
	Down UMETA(DisplayName = "Down", ToolTip="Down from Transform/FQuat/Rotator, or raw vector."),
	Euler UMETA(DisplayName = "Euler", ToolTip="Fetch Euler from Transform.GetRotation/FQuat/Rotator."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Axis Alignment Selector"))
enum class EPCGExAxisAlign : uint8
{
	Forward UMETA(DisplayName = "Forward", ToolTip="..."),
	Backward UMETA(DisplayName = "Backward", ToolTip="..."),
	Right UMETA(DisplayName = "Right", ToolTip="..."),
	Left UMETA(DisplayName = "Left", ToolTip="..."),
	Up UMETA(DisplayName = "Up", ToolTip="..."),
	Down UMETA(DisplayName = "Down", ToolTip="..."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Extent Type Selector"))
enum class EPCGExExtension : uint8
{
	None UMETA(DisplayName = "None", ToolTip="No Extension"),
	Extents UMETA(DisplayName = "Extents", ToolTip="Extents"),
	Scale UMETA(DisplayName = "Scale", ToolTip="Scale"),
	ScaledExtents UMETA(DisplayName = "Scaled Extents", ToolTip="Scaled extents"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Distance Reference Selector"))
enum class EPCGExDistance : uint8
{
	Center UMETA(DisplayName = "Center", ToolTip="Center"),
	SphereBounds UMETA(DisplayName = "Sphere Bounds", ToolTip="Point sphere which radius is scaled extent"),
	BoxBounds UMETA(DisplayName = "Box Bounds", ToolTip="Point extents"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Index Safety"))
enum class EPCGExIndexSafety : uint8
{
	Ignore UMETA(DisplayName = "Ignore", Tooltip="Out of bounds indices are ignored."),
	Tile UMETA(DisplayName = "Tile", Tooltip="Out of bounds indices are tiled."),
	Clamp UMETA(DisplayName = "Clamp", Tooltip="Out of bounds indices are clamped."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Collision Type Filter"))
enum class EPCGExCollisionFilterType : uint8
{
	Channel UMETA(DisplayName = "Channel", ToolTip="Channel"),
	ObjectType UMETA(DisplayName = "Object Type", ToolTip="Object Type"),
	Profile UMETA(DisplayName = "Profile", ToolTip="Profile"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Selector Type"))
enum class EPCGExSelectorType : uint8
{
	SingleField UMETA(DisplayName = "Single Field", ToolTip="Forward from Transform/FQuat/Rotator, or raw vector."),
	Direction UMETA(DisplayName = "Direction", ToolTip="Backward from Transform/FQuat/Rotator, or raw vector."),
};


UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Range Type"))
enum class EPCGExRangeType : uint8
{
	FullRange UMETA(DisplayName = "Full Range", ToolTip="Normalize in the [0..1] range using [0..Max Value] range."),
	EffectiveRange UMETA(DisplayName = "Effective Range", ToolTip="Remap the input [Min..Max] range to [0..1]."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Truncate Mode"))
enum class EPCGExTruncateMode : uint8
{
	None UMETA(DisplayName = "None", ToolTip="None"),
	Round UMETA(DisplayName = "Round", ToolTip="Round"),
	Ceil UMETA(DisplayName = "Ceil", ToolTip="Ceil"),
	Floor UMETA(DisplayName = "Floor", ToolTip="Floor"),
};

namespace PCGEx
{
	const FName SourcePointsLabel = TEXT("In");
	const FName SourceTargetsLabel = TEXT("InTargets");
	const FName OutputPointsLabel = TEXT("Out");

	constexpr FLinearColor NodeColorDebug = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
	constexpr FLinearColor NodeColorGraph = FLinearColor(80.0f / 255.0f, 241.0f / 255.0f, 168.0f / 255.0f, 1.0f);
	constexpr FLinearColor NodeColorExParam = FLinearColor(254.0f / 255.0f, 132.0f / 255.0f, 0.1f / 255.0f, 1.0f);
	constexpr FLinearColor NodeColorFilter = FLinearColor(21.0f / 255.0f, 193.0f / 255.0f, 33.0f / 255.0f, 1.0f);
	constexpr FLinearColor NodeColorPathfinding = FLinearColor(80.0f / 255.0f, 241.0f / 255.0f, 100.0f / 255.0f, 1.0f);
	constexpr FLinearColor NodeColorEdge = FLinearColor(100.0f / 255.0f, 241.0f / 255.0f, 100.0f / 255.0f, 1.0f);
	constexpr FLinearColor NodeColorPath = FLinearColor(50.0f / 255.0f, 150.0f / 255.0f, 241.0f / 255.0f, 1.0f);
	constexpr FLinearColor NodeColorPrimitives = FLinearColor(35.0f / 255.0f, 253.0f / 255.0f, 113.0f / 255.0f, 1.0f);
	constexpr FLinearColor NodeColorWhite = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	const FSoftObjectPath DefaultDotOverDistanceCurve = FSoftObjectPath(TEXT("/PCGExtendedToolkit/Curves/FC_PCGExGraphBalance_DistanceOnly.FC_PCGExGraphBalance_DistanceOnly"));
	const FSoftObjectPath WeightDistributionLinearInv = FSoftObjectPath(TEXT("/PCGExtendedToolkit/Curves/FC_PCGExWeightDistribution_Linear_Inv.FC_PCGExWeightDistribution_Linear_Inv"));
	const FSoftObjectPath WeightDistributionLinear = FSoftObjectPath(TEXT("/PCGExtendedToolkit/Curves/FC_PCGExWeightDistribution_Linear.FC_PCGExWeightDistribution_Linear"));
	const FSoftObjectPath WeightDistributionExpoInv = FSoftObjectPath(TEXT("/PCGExtendedToolkit/Curves/FC_PCGExWeightDistribution_Expo_Inv.FC_PCGExWeightDistribution_Expo_Inv"));
	const FSoftObjectPath WeightDistributionExpo = FSoftObjectPath(TEXT("/PCGExtendedToolkit/Curves/FC_PCGExWeightDistribution_Expo.FC_PCGExWeightDistribution_Expo"));
	const FSoftObjectPath SteepnessWeightCurve = FSoftObjectPath(TEXT("/PCGExtendedToolkit/Curves/FC_PCGExSteepness_Default.FC_PCGExSteepness_Default"));

	static bool IsValidName(const FName Name) { return FPCGMetadataAttributeBase::IsValidName(Name) && !Name.IsNone(); }

	static void ArrayOfIndices(TArray<int32>& OutArray, const int32 InNum)
	{
		OutArray.SetNum(InNum);
		for (int i = 0; i < InNum; i++) { OutArray[i] = i; }
	}

	static FName GetCompoundName(const FName A, const FName B)
	{
		// PCGEx/A/B
		const FString Separator = TEXT("/");
		return *(TEXT("PCGEx") + Separator + A.ToString() + Separator + B.ToString());
	}

	static FName GetCompoundName(const FName A, const FName B, const FName C)
	{
		// PCGEx/A/B/C
		const FString Separator = TEXT("/");
		return *(TEXT("PCGEx") + Separator + A.ToString() + Separator + B.ToString() + Separator + C.ToString());
	}

	// Unsigned uint64 hash
	static uint64 H64U(const uint32 A, const uint32 B)
	{
		return A > B ?
			       static_cast<uint64>(A) << 32 | B :
			       static_cast<uint64>(B) << 32 | A;
	}

	// Signed uint64 hash
	static uint64 H64(const uint32 A, const uint32 B) { return static_cast<uint64>(A) << 32 | B; }

	// Expand uint64 hash
	static uint32 H64A(const uint64 Hash) { return static_cast<uint32>(Hash >> 32); }
	static uint32 H64B(const uint64 Hash) { return static_cast<uint32>(Hash); }

	static void H64(const uint64 Hash, uint32& A, uint32& B)
	{
		A = H64A(Hash);
		B = H64B(Hash);
	}

	static uint64 H6416(const uint16 A, const uint16 B, const uint16 C, const uint16 D)
	{
		return (static_cast<uint64>(A) << 48) |
			(static_cast<uint64>(B) << 32) |
			(static_cast<uint64>(C) << 16) |
			static_cast<uint64>(D);
	}

	static void H6416(const uint64_t H, uint16& A, uint16& B, uint16& C, uint16& D)
	{
		A = static_cast<uint16>(H >> 48);
		B = static_cast<uint16>((H >> 32) & 0xFFFF);
		C = static_cast<uint16>((H >> 16) & 0xFFFF);
		D = static_cast<uint16>(H & 0xFFFF);
	}

	static uint64 H64S(const uint32 A, const uint32 B, const uint32 C)
	{
		uint64 H = (static_cast<uint64>(A) << 32) | (static_cast<uint64>(B) << 16) | C;

		H ^= (H >> 23) ^ (H << 37);
		H *= 0xc4ceb9fe1a85ec53;
		H ^= (H >> 32);

		return H;
	}

	static uint64 H64S(int32 ABC[3]) { return H64S(ABC[0], ABC[1], ABC[2]); }

#pragma region Field Helpers

#pragma region Meta types
	static EPCGMetadataTypes GetPointPropertyTypeId(const EPCGPointProperties Property)
	{
		switch (Property)
		{
		case EPCGPointProperties::Density:
			return EPCGMetadataTypes::Float;
		case EPCGPointProperties::BoundsMin:
			return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::BoundsMax:
			return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::Extents:
			return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::Color:
			return EPCGMetadataTypes::Vector4;
		case EPCGPointProperties::Position:
			return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::Rotation:
			return EPCGMetadataTypes::Quaternion;
		case EPCGPointProperties::Scale:
			return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::Transform:
			return EPCGMetadataTypes::Transform;
		case EPCGPointProperties::Steepness:
			return EPCGMetadataTypes::Float;
		case EPCGPointProperties::LocalCenter:
			return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::Seed:
			return EPCGMetadataTypes::Integer32;
		default:
			return EPCGMetadataTypes::Unknown;
		}
	}

#pragma endregion

	static const TMap<FString, EPCGExTransformComponent> STRMAP_TRANSFORM_FIELD = {
		{TEXT("POSITION"), EPCGExTransformComponent::Position},
		{TEXT("POS"), EPCGExTransformComponent::Position},
		{TEXT("ROTATION"), EPCGExTransformComponent::Rotation},
		{TEXT("ROT"), EPCGExTransformComponent::Rotation},
		{TEXT("ORIENT"), EPCGExTransformComponent::Rotation},
		{TEXT("SCALE"), EPCGExTransformComponent::Scale},
	};

	static const TMap<FString, EPCGExSingleField> STRMAP_SINGLE_FIELD = {
		{TEXT("X"), EPCGExSingleField::X},
		{TEXT("R"), EPCGExSingleField::X},
		{TEXT("ROLL"), EPCGExSingleField::X},
		{TEXT("RX"), EPCGExSingleField::X},
		{TEXT("Y"), EPCGExSingleField::Y},
		{TEXT("G"), EPCGExSingleField::Y},
		{TEXT("YAW"), EPCGExSingleField::Y},
		{TEXT("RY"), EPCGExSingleField::Y},
		{TEXT("Z"), EPCGExSingleField::Z},
		{TEXT("B"), EPCGExSingleField::Z},
		{TEXT("P"), EPCGExSingleField::Z},
		{TEXT("PITCH"), EPCGExSingleField::Z},
		{TEXT("RZ"), EPCGExSingleField::Z},
		{TEXT("W"), EPCGExSingleField::W},
		{TEXT("A"), EPCGExSingleField::W},
		{TEXT("L"), EPCGExSingleField::Length},
		{TEXT("LEN"), EPCGExSingleField::Length},
		{TEXT("LENGTH"), EPCGExSingleField::Length},
	};

	static const TMap<FString, EPCGExAxis> STRMAP_AXIS = {
		{TEXT("FORWARD"), EPCGExAxis::Forward},
		{TEXT("FRONT"), EPCGExAxis::Forward},
		{TEXT("BACKWARD"), EPCGExAxis::Backward},
		{TEXT("BACK"), EPCGExAxis::Backward},
		{TEXT("RIGHT"), EPCGExAxis::Right},
		{TEXT("LEFT"), EPCGExAxis::Left},
		{TEXT("UP"), EPCGExAxis::Up},
		{TEXT("TOP"), EPCGExAxis::Up},
		{TEXT("DOWN"), EPCGExAxis::Down},
		{TEXT("BOTTOM"), EPCGExAxis::Down},
	};

	static bool GetComponentSelection(const TArray<FString>& Names, EPCGExTransformComponent& OutSelection)
	{
		if (Names.IsEmpty()) { return false; }
		for (const FString& Name : Names)
		{
			if (const EPCGExTransformComponent* Selection = STRMAP_TRANSFORM_FIELD.Find(Name.ToUpper()))
			{
				OutSelection = *Selection;
				return true;
			}
		}
		return false;
	}

	static bool GetFieldSelection(const TArray<FString>& Names, EPCGExSingleField& OutSelection)
	{
		if (Names.IsEmpty()) { return false; }
		const FString& STR = Names.Num() > 1 ? Names[1].ToUpper() : Names[0].ToUpper();
		if (const EPCGExSingleField* Selection = STRMAP_SINGLE_FIELD.Find(STR))
		{
			OutSelection = *Selection;
			return true;
		}
		if (STR.Len() <= 0) { return false; }
		if (const EPCGExSingleField* Selection = STRMAP_SINGLE_FIELD.Find(FString::Printf(TEXT("%c"), STR[0]).ToUpper()))
		{
			OutSelection = *Selection;
			return true;
		}
		return false;
	}

	static bool GetAxisSelection(const TArray<FString>& Names, EPCGExAxis& OutSelection)
	{
		if (Names.IsEmpty()) { return false; }
		for (const FString& Name : Names)
		{
			if (const EPCGExAxis* Selection = STRMAP_AXIS.Find(Name.ToUpper()))
			{
				OutSelection = *Selection;
				return true;
			}
		}
		return false;
	}

#pragma endregion

	struct PCGEXTENDEDTOOLKIT_API FPointRef
	{
		friend struct PCGExData::FPointIO;

		FPointRef(const FPCGPoint& InPoint, const int32 InIndex):
			Point(&InPoint), Index(InIndex)
		{
		}

		FPointRef(const FPCGPoint* InPoint, const int32 InIndex):
			Point(InPoint), Index(InIndex)
		{
		}

		FPointRef(const FPointRef& Other):
			Point(Other.Point), Index(Other.Index)
		{
		}

		bool IsValid() const { return Point && Index != -1; }
		const FPCGPoint* Point = nullptr;
		const int32 Index = -1;

		FPCGPoint& MutablePoint() const { return const_cast<FPCGPoint&>(*Point); }
	};

	static UWorld* GetWorld(const FPCGContext* Context)
	{
		check(Context->SourceComponent.IsValid());
		return Context->SourceComponent->GetWorld();
	}

	template <typename T>
	static void Swap(TArray<T>& Array, int32 FirstIndex, int32 SecondIndex)
	{
		T* Ptr1 = &Array[FirstIndex];
		T* Ptr2 = &Array[SecondIndex];
		std::swap(*Ptr1, *Ptr2);
	}
}
