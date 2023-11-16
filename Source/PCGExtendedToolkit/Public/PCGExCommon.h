// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include <iostream>

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "HAL/CriticalSection.h"
#include <mutex>
#include <shared_mutex>

#include "PCGComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
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
enum class EPCGExSingleFieldSelection : uint8
{
	X UMETA(DisplayName = "X/Roll", ToolTip="X/Roll component if it exist, raw value otherwise."),
	Y UMETA(DisplayName = "Y/Pitch", ToolTip="Y/Pitch component if it exist, fallback to previous value otherwise."),
	Z UMETA(DisplayName = "Z/Yaw", ToolTip="Z/Yaw component if it exist, fallback to previous value otherwise."),
	W UMETA(DisplayName = "W", ToolTip="W component if it exist, fallback to previous value otherwise."),
	Length UMETA(DisplayName = "Length", ToolTip="Length if vector, raw value otherwise."),
};

UENUM(BlueprintType)
enum class EPCGExDirectionSelection : uint8
{
	Forward UMETA(DisplayName = "Forward", ToolTip="Forward from Transform/FQuat/Rotator, or raw vector."),
	Backward UMETA(DisplayName = "Backward", ToolTip="Backward from Transform/FQuat/Rotator, or raw vector."),
	Right UMETA(DisplayName = "Right", ToolTip="Right from Transform/FQuat/Rotator, or raw vector."),
	Left UMETA(DisplayName = "Left", ToolTip="Left from Transform/FQuat/Rotator, or raw vector."),
	Up UMETA(DisplayName = "Up", ToolTip="Up from Transform/FQuat/Rotator, or raw vector."),
	Down UMETA(DisplayName = "Down", ToolTip="Down from Transform/FQuat/Rotator, or raw vector."),
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputSelector
{
	GENERATED_BODY()

	FPCGExInputSelector()
	{
		bValidatedAtLeastOnce = false;
	}

	FPCGExInputSelector(const FPCGExInputSelector& Other)
	{
		Selector = Other.Selector;
		Attribute = Other.Attribute;
		bValidatedAtLeastOnce = false;
	}

public:
	/** Point Attribute or $Property */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector Selector;

	FPCGMetadataAttributeBase* Attribute = nullptr;
	bool bValidatedAtLeastOnce = false;
	int16 UnderlyingType = 0;

	/**
	 * Check if the current selector can currently read from a given UPCGPointData.
	 * Requires Validate() to be called first.
	 * @param PointData 
	 * @return 
	 */
	bool IsValid(const UPCGPointData* PointData) const
	{
		const EPCGAttributePropertySelection Sel = Selector.GetSelection();
		if (Sel == EPCGAttributePropertySelection::Attribute && (Attribute == nullptr || !PointData->Metadata->HasAttribute(Selector.GetName()))) { return false; }
		return Selector.IsValid();
	}

	/**
	 * Try to read typed value from the cached attribute, if it exists.
	 * Use Validate() to temporarily cache attribute for a given UPCGPointData
	 * @tparam T 
	 * @param MetadataEntry 
	 * @param OutValue 
	 * @return 
	 */
	template <typename T>
	bool TryGetValue(PCGMetadataEntryKey MetadataEntry, T& OutValue)
	{
		if (Attribute == nullptr) { return false; }
		OutValue = static_cast<FPCGMetadataAttribute<T>*>(Attribute)->GetValueFromItemKey(MetadataEntry);
		return true;
	}

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

	/**
	 * Validate & cache the current selector for a given UPCGPointData
	 * @param InData 
	 * @return 
	 */
	bool Validate(const UPCGPointData* InData)
	{
		bValidatedAtLeastOnce = true;
		Selector = Selector.CopyAndFixLast(InData);
		if (Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			Attribute = Selector.IsValid() ? InData->Metadata->GetMutableAttribute(Selector.GetName()) : nullptr;
			if (Attribute)
			{
				const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InData, Selector);
				UnderlyingType = Accessor->GetUnderlyingType();
				//if (!Accessor.IsValid()) { Attribute = nullptr; }
			}
			return Attribute != nullptr;
		}
		else
		{
			const bool bTempIsValid = Selector.IsValid();
			if (bTempIsValid)
			{
				const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InData, Selector);
				UnderlyingType = Accessor->GetUnderlyingType();
			}
			return bTempIsValid;
		}
	}

	FString ToString() const { return Selector.GetName().ToString(); }

	~FPCGExInputSelector()
	{
		Attribute = nullptr;
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputSelectorWithField : public FPCGExInputSelector
{
	GENERATED_BODY()

	FPCGExInputSelectorWithField(): FPCGExInputSelector()
	{
	}

	FPCGExInputSelectorWithField(const FPCGExInputSelectorWithField& Other)
		: FPCGExInputSelector(Other)
	{
		FieldSelection = Other.FieldSelection;
	}

public:
	/** Sub-component order, used only for multi-field attributes (FVector, FRotator etc). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayAfter="Selector"))
	EPCGExOrderedFieldSelection FieldSelection = EPCGExOrderedFieldSelection::XYZ;

	~FPCGExInputSelectorWithField()
	{
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputSelectorWithDirection : public FPCGExInputSelector
{
	GENERATED_BODY()

	FPCGExInputSelectorWithDirection(): FPCGExInputSelector()
	{
	}

	FPCGExInputSelectorWithDirection(const FPCGExInputSelectorWithDirection& Other)
		: FPCGExInputSelector(Other)
	{
		Direction = Other.Direction;
	}

public:
	/** Sub-component order, used only for multi-field attributes (FVector, FRotator etc). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayAfter="Selector"))
	EPCGExDirectionSelection Direction = EPCGExDirectionSelection::Forward;

	~FPCGExInputSelectorWithDirection()
	{
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputSelectorWithSingleField : public FPCGExInputSelector
{
	GENERATED_BODY()

	FPCGExInputSelectorWithSingleField(): FPCGExInputSelector()
	{
	}

	FPCGExInputSelectorWithSingleField(const FPCGExInputSelectorWithSingleField& Other)
		: FPCGExInputSelector(Other)
	{
		FieldSelection = Other.FieldSelection;
		Direction = Other.Direction;
	}

public:
	/** Single field selection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayAfter="Selector"))
	EPCGExSingleFieldSelection FieldSelection = EPCGExSingleFieldSelection::X;

	/** Direction to sample on relevant data types (FQuat are transformed to a direction first, from which the single component is selected) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayAfter="Selector"))
	EPCGExDirectionSelection Direction = EPCGExDirectionSelection::Forward;

	~FPCGExInputSelectorWithSingleField()
	{
	}
};

namespace PCGEx
{

	const FName SourcePointsLabel = TEXT("InPoints");
	const FName OutputPointsLabel = TEXT("OutPoints");

	const FSoftObjectPath DefaultDotOverDistanceCurve = FSoftObjectPath(TEXT("/PCGExtendedToolkit/FC_PCGEx_DotPriorityBalanced.FC_PCGEx_DotPriorityBalanced"));
	
	class Common
	{
	public:
		
		static UWorld* GetWorld(const FPCGContext* Context)
		{
			check(Context->SourceComponent.IsValid());
			return Context->SourceComponent->GetWorld();
		}
		
		static bool ForEachPointData(
			FPCGContext* Context,
			TArray<FPCGTaggedData>& Sources,
			const TFunction<void(const FPCGTaggedData& Source, const UPCGPointData* PointData)>& PerPointDataFunc)
		{
			bool bSkippedInvalidData = false;
			for (const FPCGTaggedData& Source : Sources)
			{
				const UPCGSpatialData* AsSpatialData = Cast<UPCGSpatialData>(Source.Data);
				if (!AsSpatialData)
				{
					bSkippedInvalidData = true;
					continue;
				}

				const UPCGPointData* AsPointData = AsSpatialData->ToPointData(Context);
				if (!AsPointData)
				{
					bSkippedInvalidData = true;
					continue;
				}

				PerPointDataFunc(Source, AsPointData);
			}
			return bSkippedInvalidData;
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

		static FVector GetDirection(const FQuat& Quat, const EPCGExDirectionSelection Dir)
		{
			switch (Dir)
			{
			default:
			case EPCGExDirectionSelection::Forward:
				return Quat.GetForwardVector();
			case EPCGExDirectionSelection::Backward:
				return Quat.GetForwardVector() * -1;
			case EPCGExDirectionSelection::Right:
				return Quat.GetRightVector();
			case EPCGExDirectionSelection::Left:
				return Quat.GetRightVector() * -1;
			case EPCGExDirectionSelection::Up:
				return Quat.GetUpVector();
			case EPCGExDirectionSelection::Down:
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

		// Remap function
		static double Remap(const double Value, const double Min, const double Max, const double NewMin = 0, const double NewMax = 1) {
			return NewMin + ((Value - Min) / (Max - Min)) * (NewMax - NewMin);
		}

	};
}
