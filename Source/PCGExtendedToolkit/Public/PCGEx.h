// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGExMath.h"
#include "MatchAndSet/PCGMatchAndSetWeighted.h"

#include "PCGEx.generated.h"

#pragma region MACROS

#define FTEXT(_TEXT) FText::FromString(FString(_TEXT))
#define FSTRING(_TEXT) FString(_TEXT)

#define PCGEX_DELETE(_VALUE) delete _VALUE; _VALUE = nullptr;
#define PCGEX_DELETE_TARRAY(_VALUE) for(const auto* Item : _VALUE){ delete Item; } _VALUE.Empty();
#define PCGEX_CLEANUP(_VALUE) _VALUE.Cleanup();

#define PCGEX_SORTED_ADD(_TARRAY, _ITEM, _COMPARE)\
int32 HeapIndex = 0; for (int i = HeapIndex; i >= 0; i--){ if (_COMPARE){ HeapIndex = i + 1; break;	}}\
if (HeapIndex >= _TARRAY.Num()) { _TARRAY.Add(_ITEM); }\
else { _TARRAY.Insert(_ITEM, HeapIndex); }

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
enum class EPCGExTransformComponent : uint8
{
	Position UMETA(DisplayName = "Position", ToolTip="Position component."),
	Rotation UMETA(DisplayName = "Rotation", ToolTip="Rotation component."),
	Scale UMETA(DisplayName = "Scale", ToolTip="Scale component."),
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
	Euler UMETA(DisplayName = "Euler", ToolTip="Fetch Euler from Transform.GetRotation/FQuat/Rotator."),
};

UENUM(BlueprintType)
enum class EPCGExExtension : uint8
{
	None UMETA(DisplayName = "None", ToolTip="No Extension"),
	Extents UMETA(DisplayName = "Extents", ToolTip="Extents"),
	Scale UMETA(DisplayName = "Scale", ToolTip="Scale"),
	ScaledExtents UMETA(DisplayName = "Scaled Extents", ToolTip="Scaled extents"),
};

UENUM(BlueprintType)
enum class EPCGExIndexSafety : uint8
{
	Ignore UMETA(DisplayName = "Ignore", Tooltip="Out of bounds indices are ignored."),
	Wrap UMETA(DisplayName = "Wrap", Tooltip="Out of bounds indices are wrapped."),
	Clamp UMETA(DisplayName = "Clamp", Tooltip="Out of bounds indices are clamped."),
};

UENUM(BlueprintType)
enum class EPCGExCollisionFilterType : uint8
{
	Channel UMETA(DisplayName = "Channel", ToolTip="Channel"),
	ObjectType UMETA(DisplayName = "Object Type", ToolTip="Object Type"),
	Profile UMETA(DisplayName = "Profile", ToolTip="Profile"),
};

UENUM(BlueprintType)
enum class EPCGExSelectorType : uint8
{
	SingleField UMETA(DisplayName = "Single Field", ToolTip="Forward from Transform/FQuat/Rotator, or raw vector."),
	Direction UMETA(DisplayName = "Direction", ToolTip="Backward from Transform/FQuat/Rotator, or raw vector."),
};

namespace PCGEx
{
	const FName SourcePointsLabel = TEXT("In");
	const FName SourceTargetsLabel = TEXT("InTargets");
	const FName OutputPointsLabel = TEXT("Out");

	constexpr FLinearColor NodeColorDebug = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
	constexpr FLinearColor NodeColorGraph = FLinearColor(80.0f / 255.0f, 241.0f / 255.0f, 168.0f / 255.0f, 1.0f);
	constexpr FLinearColor NodeColorPathfinding = FLinearColor(80.0f / 255.0f, 241.0f / 255.0f, 100.0f / 255.0f, 1.0f);
	constexpr FLinearColor NodeColorEdge = FLinearColor(100.0f / 255.0f, 241.0f / 255.0f, 100.0f / 255.0f, 1.0f);
	constexpr FLinearColor NodeColorPath = FLinearColor(50.0f / 255.0f, 150.0f / 255.0f, 241.0f / 255.0f, 1.0f);
	constexpr FLinearColor NodeColorPrimitives = FLinearColor(35.0f / 255.0f, 253.0f / 255.0f, 113.0f / 255.0f, 1.0f);
	constexpr FLinearColor NodeColorWhite = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	const FSoftObjectPath DefaultDotOverDistanceCurve = FSoftObjectPath(TEXT("/PCGExtendedToolkit/FC_PCGExGraphBalance_DistanceOnly.FC_PCGExGraphBalance_DistanceOnly"));
	const FSoftObjectPath WeightDistributionLinearInv = FSoftObjectPath(TEXT("/PCGExtendedToolkit/FC_PCGExWeightDistribution_Linear_Inv.FC_PCGExWeightDistribution_Linear_Inv"));
	const FSoftObjectPath WeightDistributionLinear = FSoftObjectPath(TEXT("/PCGExtendedToolkit/FC_PCGExWeightDistribution_Linear.FC_PCGExWeightDistribution_Linear"));
	const FSoftObjectPath WeightDistributionExpoInv = FSoftObjectPath(TEXT("/PCGExtendedToolkit/FC_PCGExWeightDistribution_Expo_Inv.FC_PCGExWeightDistribution_Expo_Inv"));
	const FSoftObjectPath WeightDistributionExpo = FSoftObjectPath(TEXT("/PCGExtendedToolkit/FC_PCGExWeightDistribution_Expo.FC_PCGExWeightDistribution_Expo"));

#pragma region Field Helpers

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
	static T SanitizeIndex(const T& Index, const T& Limit, const EPCGExIndexSafety Method)
	{
		switch (Method)
		{
		case EPCGExIndexSafety::Ignore:
			if (Index < 0 || Index > Limit) { return -1; }
			break;
		case EPCGExIndexSafety::Wrap:
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

	static void RandomizeSeed(FPCGPoint& Point)
	{
		Point.Seed = static_cast<int32>(PCGExMath::Remap(
			FMath::PerlinNoise3D(PCGExMath::Tile(Point.Transform.GetLocation() * 0.001, FVector(-1), FVector(1))),
			-1, 1, TNumericLimits<int32>::Min(), TNumericLimits<int32>::Max()));
	}
}
