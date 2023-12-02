// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGComponent.h"
#include "PCGContext.h"

#include "PCGEx.generated.h"

#pragma region MACROS

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
MACRO(FVector2D, Vector2)  \
MACRO(FVector, Vector)    \
MACRO(FVector4, Vector4)   \
MACRO(FQuat, Quaternion)      \
MACRO(FRotator, Rotator)   \
MACRO(FTransform, Transform) \
MACRO(FString, String)    \
MACRO(FName, Name)

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
	Channel UMETA(DisplayName = "Channel", ToolTip="TBD"),
	ObjectType UMETA(DisplayName = "Object Type", ToolTip="TBD"),
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
	constexpr FLinearColor NodeColorSpline = FLinearColor(50.0f / 255.0f, 150.0f / 255.0f, 241.0f / 255.0f, 1.0f);
	constexpr FLinearColor NodeColorWhite = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	const FSoftObjectPath DefaultDotOverDistanceCurve = FSoftObjectPath(TEXT("/PCGExtendedToolkit/FC_PCGExGraphBalance_DistanceOnly.FC_PCGExGraphBalance_DistanceOnly"));
	const FSoftObjectPath WeightDistributionLinear = FSoftObjectPath(TEXT("/PCGExtendedToolkit/FC_PCGExWeightDistribution_Linear.FC_PCGExWeightDistribution_Linear"));

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
			return FMath::Wrap(Index, 0, Limit);
			break;
		case EPCGExIndexSafety::Clamp:
			return FMath::Clamp(Index, 0, Limit);
			break;
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
}
