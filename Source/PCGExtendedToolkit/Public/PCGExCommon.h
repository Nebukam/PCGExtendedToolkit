// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "PCGExCommon.generated.h"

#define PCGEX_FOREACH_POINTPROPERTY(MACRO)\
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
MACRO(EPCGPointProperties::Seed, Seed) \

#define PCGEX_FOREACH_POINTEXTRAPROPERTY(MACRO)\
MACRO(EPCGExtraProperties::Index, MetadataEntry) \

UENUM(BlueprintType)
enum class EComponentSelection : uint8
{
	X UMETA(DisplayName = "X"),
	Y UMETA(DisplayName = "Y (→x)"),
	Z UMETA(DisplayName = "Z (→y)"),
	W UMETA(DisplayName = "W (→z)"),
	XYZ UMETA(DisplayName = "X→Y→Z"),
	XZY UMETA(DisplayName = "X→Z→Y"),
	YXZ UMETA(DisplayName = "Y→X→Z"),
	YZX UMETA(DisplayName = "Y→Z→X"),
	ZXY UMETA(DisplayName = "Z→X→Y"),
	ZYX UMETA(DisplayName = "Z→Y→X"),
	Length UMETA(DisplayName = "Length"),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSelectorSettingsBase
{
	GENERATED_BODY()

	FPCGExSelectorSettingsBase()
	{
	}

	FPCGExSelectorSettingsBase(const FPCGExSelectorSettingsBase& Other)
	{
		Selector = Other.Selector;
		ComponentSelection = Other.ComponentSelection;
		Attribute = Other.Attribute;
	}

public:
	/** Name of the attribute to compare */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector Selector;

	/** Sub-sorting order, used only for multi-field attributes (FVector, FRotator etc). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EComponentSelection ComponentSelection = EComponentSelection::XYZ;

	/* Can hold a reference to the attribute pointer, if prepared like so */
	FPCGMetadataAttributeBase* Attribute = nullptr;

	bool IsValid(const UPCGPointData* PointData) const
	{
		const EPCGAttributePropertySelection Sel = Selector.GetSelection();
		if (Sel == EPCGAttributePropertySelection::Attribute && (Attribute == nullptr || !PointData->Metadata->HasAttribute(Selector.GetName()))) { return false; }
		return Selector.IsValid();
	}

	template <typename T>
	bool TryGetValue(int64 Key, T& OutValue)
	{
		if (Attribute == nullptr) { return false; }
		OutValue = static_cast<FPCGMetadataAttribute<T>*>(Attribute)->GetValue(Key);
		return true;
	}

	bool CopyAndFixLast(const UPCGPointData* InData)
	{
		Selector = Selector.CopyAndFixLast(InData);
		if (Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			Attribute = Selector.IsValid() ? InData->Metadata->GetMutableAttribute(Selector.GetName()) : nullptr;
			return Attribute != nullptr;
		}
		else
		{
			return Selector.IsValid();
		}
	}

	FString ToString() const
	{
		return Selector.GetName().ToString();
	}
};

class FPCGExCommon
{
public:
	template <typename T, typename K>
	static FPCGMetadataAttribute<T>* GetTypedAttribute(const K& Settings)
	{
		if (Settings.Attribute == nullptr) { return nullptr; }
		return static_cast<FPCGMetadataAttribute<T>*>(Settings.Attribute);
	}

	template <typename T, typename K>
	static FPCGMetadataAttribute<T>* GetTypedAttribute(const K* Settings)
	{
		if (Settings->Attribute == nullptr) { return nullptr; }
		return static_cast<FPCGMetadataAttribute<T>*>(Settings->Attribute);
	}
};
