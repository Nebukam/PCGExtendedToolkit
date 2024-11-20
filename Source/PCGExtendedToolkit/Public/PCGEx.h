// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Misc/ScopeRWLock.h"
#include "UObject/ObjectPtr.h"
#include "Templates/SharedPointer.h"
#include "Templates/SharedPointerFwd.h"
#include "Curves/CurveFloat.h"
#include "Math/GenericOctree.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Actor.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "StaticMeshResources.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGExH.h"
#include "MatchAndSet/PCGMatchAndSetWeighted.h"
#include "Metadata/PCGMetadataAttribute.h"

#include "PCGExMacros.h"

#include "PCGEx.generated.h"

UENUM()
enum class EPCGExTransformComponent : uint8
{
	Position = 0 UMETA(DisplayName = "Position", ToolTip="Position component."),
	Rotation = 1 UMETA(DisplayName = "Rotation", ToolTip="Rotation component."),
	Scale    = 2 UMETA(DisplayName = "Scale", ToolTip="Scale component."),
};

UENUM()
enum class EPCGExMinimalAxis : uint8
{
	None = 0 UMETA(DisplayName = "None", ToolTip="None"),
	X    = 1 UMETA(DisplayName = "X", ToolTip="X Axis"),
	Y    = 2 UMETA(DisplayName = "Y", ToolTip="Y Axis"),
	Z    = 3 UMETA(DisplayName = "Z", ToolTip="Z Axis"),
};

UENUM()
enum class EPCGExSingleField : uint8
{
	X      = 0 UMETA(DisplayName = "X/Roll", ToolTip="X/Roll component if it exist, raw value otherwise."),
	Y      = 1 UMETA(DisplayName = "Y/Pitch", ToolTip="Y/Pitch component if it exist, fallback to previous value otherwise."),
	Z      = 2 UMETA(DisplayName = "Z/Yaw", ToolTip="Z/Yaw component if it exist, fallback to previous value otherwise."),
	W      = 3 UMETA(DisplayName = "W", ToolTip="W component if it exist, fallback to previous value otherwise."),
	Length = 4 UMETA(DisplayName = "Length", ToolTip="Length if vector, raw value otherwise."),
};

UENUM()
enum class EPCGExAxis : uint8
{
	Forward  = 0 UMETA(DisplayName = "Default (Forward)", ToolTip="Forward from Transform/FQuat/Rotator, or raw vector."),
	Backward = 1 UMETA(DisplayName = "Backward", ToolTip="Backward from Transform/FQuat/Rotator, or raw vector."),
	Right    = 2 UMETA(DisplayName = "Right", ToolTip="Right from Transform/FQuat/Rotator, or raw vector."),
	Left     = 3 UMETA(DisplayName = "Left", ToolTip="Left from Transform/FQuat/Rotator, or raw vector."),
	Up       = 4 UMETA(DisplayName = "Up", ToolTip="Up from Transform/FQuat/Rotator, or raw vector."),
	Down     = 5 UMETA(DisplayName = "Down", ToolTip="Down from Transform/FQuat/Rotator, or raw vector."),
};

UENUM()
enum class EPCGExAxisAlign : uint8
{
	Forward  = 0 UMETA(DisplayName = "Forward", ToolTip="..."),
	Backward = 1 UMETA(DisplayName = "Backward", ToolTip="..."),
	Right    = 2 UMETA(DisplayName = "Right", ToolTip="..."),
	Left     = 3 UMETA(DisplayName = "Left", ToolTip="..."),
	Up       = 4 UMETA(DisplayName = "Up", ToolTip="..."),
	Down     = 5 UMETA(DisplayName = "Down", ToolTip="..."),
};

UENUM()
enum class EPCGExDistance : uint8
{
	Center       = 0 UMETA(DisplayName = "Center", ToolTip="Center"),
	SphereBounds = 1 UMETA(DisplayName = "Sphere Bounds", ToolTip="Point sphere which radius is scaled extent"),
	BoxBounds    = 2 UMETA(DisplayName = "Box Bounds", ToolTip="Point extents"),
	None         = 3 UMETA(Hidden, DisplayName = "None", ToolTip="Used for union blending with full weight."),
};

UENUM()
enum class EPCGExIndexSafety : uint8
{
	Ignore = 0 UMETA(DisplayName = "Ignore", Tooltip="Out of bounds indices are ignored."),
	Tile   = 1 UMETA(DisplayName = "Tile", Tooltip="Out of bounds indices are tiled."),
	Clamp  = 2 UMETA(DisplayName = "Clamp", Tooltip="Out of bounds indices are clamped."),
	Yoyo   = 3 UMETA(DisplayName = "Yoyo", Tooltip="Out of bounds indices are mirrored and back."),
};

UENUM()
enum class EPCGExCollisionFilterType : uint8
{
	Channel    = 0 UMETA(DisplayName = "Channel", ToolTip="Channel"),
	ObjectType = 1 UMETA(DisplayName = "Object Type", ToolTip="Object Type"),
	Profile    = 2 UMETA(DisplayName = "Profile", ToolTip="Profile"),
};

UENUM()
enum class EPCGExRangeType : uint8
{
	FullRange      = 0 UMETA(DisplayName = "Full Range", ToolTip="Normalize in the [0..1] range using [0..Max Value] range."),
	EffectiveRange = 1 UMETA(DisplayName = "Effective Range", ToolTip="Remap the input [Min..Max] range to [0..1]."),
};

UENUM()
enum class EPCGExTruncateMode : uint8
{
	None  = 0 UMETA(DisplayName = "None", ToolTip="None"),
	Round = 1 UMETA(DisplayName = "Round", ToolTip="Round"),
	Ceil  = 2 UMETA(DisplayName = "Ceil", ToolTip="Ceil"),
	Floor = 3 UMETA(DisplayName = "Floor", ToolTip="Floor"),
};

namespace PCGEx
{
	const FString PCGExPrefix = TEXT("PCGEx/");
	const FName SourcePointsLabel = TEXT("In");
	const FName SourceTargetsLabel = TEXT("Targets");
	const FName SourceBoundsLabel = TEXT("Bounds");
	const FName OutputPointsLabel = TEXT("Out");

	const FName SourceAdditionalReq = TEXT("AdditionalRequirementsFilters");

	const FName SourcePointFilters = TEXT("PointFilters");
	const FName SourceUseValueIfFilters = TEXT("UsableValueFilters");

	const FSoftObjectPath DefaultDotOverDistanceCurve = FSoftObjectPath(TEXT("/PCGExtendedToolkit/Curves/FC_PCGExGraphBalance_DistanceOnly.FC_PCGExGraphBalance_DistanceOnly"));
	const FSoftObjectPath WeightDistributionLinearInv = FSoftObjectPath(TEXT("/PCGExtendedToolkit/Curves/FC_PCGExWeightDistribution_Linear_Inv.FC_PCGExWeightDistribution_Linear_Inv"));
	const FSoftObjectPath WeightDistributionLinear = FSoftObjectPath(TEXT("/PCGExtendedToolkit/Curves/FC_PCGExWeightDistribution_Linear.FC_PCGExWeightDistribution_Linear"));
	const FSoftObjectPath WeightDistributionExpoInv = FSoftObjectPath(TEXT("/PCGExtendedToolkit/Curves/FC_PCGExWeightDistribution_Expo_Inv.FC_PCGExWeightDistribution_Expo_Inv"));
	const FSoftObjectPath WeightDistributionExpo = FSoftObjectPath(TEXT("/PCGExtendedToolkit/Curves/FC_PCGExWeightDistribution_Expo.FC_PCGExWeightDistribution_Expo"));
	const FSoftObjectPath SteepnessWeightCurve = FSoftObjectPath(TEXT("/PCGExtendedToolkit/Curves/FC_PCGExSteepness_Default.FC_PCGExSteepness_Default"));

	FORCEINLINE static bool IsPCGExAttribute(const FString& InStr) { return InStr.StartsWith(PCGExPrefix); }
	FORCEINLINE static bool IsPCGExAttribute(const FName InName) { return IsPCGExAttribute(InName.ToString()); }
	FORCEINLINE static bool IsPCGExAttribute(const FText& InText) { return IsPCGExAttribute(InText.ToString()); }

	static FName MakePCGExAttributeName(const FString& Str0) { return FName(FText::Format(FText::FromString(TEXT("{0}{1}")), FText::FromString(PCGExPrefix), FText::FromString(Str0)).ToString()); }
	static FName MakePCGExAttributeName(const FString& Str0, const FString& Str1) { return FName(FText::Format(FText::FromString(TEXT("{0}{1}/{2}")), FText::FromString(PCGExPrefix), FText::FromString(Str0), FText::FromString(Str1)).ToString()); }

	static bool IsValidName(const FName Name) { return FPCGMetadataAttributeBase::IsValidName(Name) && !Name.IsNone(); }

	static FString StringTagFromName(const FName Name)
	{
		if (Name.IsNone()) { return TEXT(""); }
		return Name.ToString().TrimStartAndEnd();
	}

	static bool IsValidStringTag(const FString& Tag)
	{
		if (Tag.TrimStartAndEnd().IsEmpty()) { return false; }
		return true;
	}

	FORCEINLINE double TruncateDbl(const double Value, const EPCGExTruncateMode Mode)
	{
		switch (Mode)
		{
		case EPCGExTruncateMode::Round: return FMath::RoundToInt(Value);
		case EPCGExTruncateMode::Ceil: return FMath::CeilToDouble(Value);
		case EPCGExTruncateMode::Floor: return FMath::FloorToDouble(Value);
		default:
		case EPCGExTruncateMode::None: return Value;
		}
	}

	FORCEINLINE static void ArrayOfIndices(TArray<int32>& OutArray, const int32 InNum)
	{
		{
			const int32 _num_ = InNum;
			OutArray.Reserve(_num_);
			OutArray.SetNum(_num_);
		}
		for (int i = 0; i < InNum; i++) { OutArray[i] = i; }
	}

	FORCEINLINE static FName GetCompoundName(const FName A, const FName B)
	{
		// PCGEx/A/B
		const FString Separator = TEXT("/");
		return *(TEXT("PCGEx") + Separator + A.ToString() + Separator + B.ToString());
	}

	FORCEINLINE static FName GetCompoundName(const FName A, const FName B, const FName C)
	{
		// PCGEx/A/B/C
		const FString Separator = TEXT("/");
		return *(TEXT("PCGEx") + Separator + A.ToString() + Separator + B.ToString() + Separator + C.ToString());
	}

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

	FORCEINLINE static UWorld* GetWorld(const FPCGContext* Context)
	{
		check(Context->SourceComponent.IsValid());
		return Context->SourceComponent->GetWorld();
	}

	static void ScopeIndices(const TArray<int32>& InIndices, TArray<uint64>& OutScopes)
	{
		TArray<int32> InIndicesCopy = InIndices;
		InIndicesCopy.Sort();

		int32 StartIndex = InIndicesCopy[0];
		int32 LastIndex = StartIndex;
		int32 Count = 1;

		for (int i = 1; i < InIndicesCopy.Num(); i++)
		{
			const int32 NextIndex = InIndicesCopy[i];
			if (NextIndex == (LastIndex + 1))
			{
				Count++;
				LastIndex = NextIndex;
				continue;
			}

			OutScopes.Emplace(H64(StartIndex, Count));
			LastIndex = StartIndex = NextIndex;
			Count = 0;
		}

		OutScopes.Emplace(H64(StartIndex, Count));
	}

	template <typename T>
	FORCEINLINE static bool SameSet(const TSet<T>& A, const TSet<T>& B)
	{
		if (A.Num() != B.Num()) { return false; }
		for (const T& Entry : A) { if (!B.IsInside(Entry)) { return false; } }
		return true;
	}

	struct FIndexedItem
	{
		int32 Index;
		FBoxSphereBounds Bounds;

		FIndexedItem(const int32 InIndex, const FBoxSphereBounds& InBounds)
			: Index(InIndex), Bounds(InBounds)
		{
		}
	};

	PCGEX_OCTREE_SEMANTICS_REF(FIndexedItem, { return Element.Bounds;}, { return A.Index == B.Index; })
}
