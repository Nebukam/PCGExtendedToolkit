// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/



#pragma once

#include "CoreMinimal.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/PCGMetadata.h"

class UPCGManagedComponent;
class UPCGData;
class UPCGComponent;

#pragma region Macros

#define PCGEX_FOREACH_SUPPORTEDTYPES(MACRO, ...) \
MACRO(float, Float, __VA_ARGS__)      \
MACRO(double, Double, __VA_ARGS__)     \
MACRO(int32, Integer32, __VA_ARGS__)      \
MACRO(int64, Integer64, __VA_ARGS__)      \
MACRO(FVector2D, Vector2, __VA_ARGS__)  \
MACRO(FVector, Vector, __VA_ARGS__)    \
MACRO(FVector4, Vector4, __VA_ARGS__)   \
MACRO(FQuat, Quaternion, __VA_ARGS__)      \
MACRO(FTransform, Transform, __VA_ARGS__) \
MACRO(FString, String, __VA_ARGS__)    \
MACRO(bool, Boolean, __VA_ARGS__)       \
MACRO(FRotator, Rotator, __VA_ARGS__)   \
MACRO(FName, Name, __VA_ARGS__)\
MACRO(FSoftObjectPath, SoftObjectPath, __VA_ARGS__)\
MACRO(FSoftClassPath, SoftClassPath, __VA_ARGS__)

#define PCGEX_INNER_FOREACH_TYPE2(_TYPE_A, _NAME_A, MACRO, ...) \
MACRO(_TYPE_A, _NAME_A, float, Float, __VA_ARGS__) \
MACRO(_TYPE_A, _NAME_A, double, Double, __VA_ARGS__) \
MACRO(_TYPE_A, _NAME_A, int32, Integer32, __VA_ARGS__) \
MACRO(_TYPE_A, _NAME_A, int64, Integer64, __VA_ARGS__) \
MACRO(_TYPE_A, _NAME_A, FVector2D, Vector2, __VA_ARGS__) \
MACRO(_TYPE_A, _NAME_A, FVector, Vector, __VA_ARGS__) \
MACRO(_TYPE_A, _NAME_A, FVector4, Vector4, __VA_ARGS__) \
MACRO(_TYPE_A, _NAME_A, FQuat, Quaternion, __VA_ARGS__) \
MACRO(_TYPE_A, _NAME_A, FTransform, Transform, __VA_ARGS__) \
MACRO(_TYPE_A, _NAME_A, FString, String, __VA_ARGS__) \
MACRO(_TYPE_A, _NAME_A, bool, Boolean, __VA_ARGS__) \
MACRO(_TYPE_A, _NAME_A, FRotator, Rotator, __VA_ARGS__) \
MACRO(_TYPE_A, _NAME_A, FName, Name, __VA_ARGS__) \
MACRO(_TYPE_A, _NAME_A, FSoftObjectPath, SoftObjectPath, __VA_ARGS__) \
MACRO(_TYPE_A, _NAME_A, FSoftClassPath, SoftClassPath, __VA_ARGS__)

#define PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(MACRO, ...) \
PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_INNER_FOREACH_TYPE2, MACRO, __VA_ARGS__)

#define PCGEX_EXECUTEWITHRIGHTTYPE_CASE(_TYPE, _NAME, MACRO) case EPCGMetadataTypes::_NAME : MACRO(_TYPE, _NAME) break;
#define PCGEX_EXECUTEWITHRIGHTTYPE(_TYPE, MACRO) switch (_TYPE){ PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_EXECUTEWITHRIGHTTYPE_CASE, MACRO) default: ; }

#define PCGEX_FOREACH_POINT_NATIVE_PROPERTY(MACRO, ...)\
MACRO(Transform, FTransform, __VA_ARGS__) \
MACRO(Density, float, __VA_ARGS__) \
MACRO(BoundsMin, FVector, __VA_ARGS__) \
MACRO(BoundsMax, FVector, __VA_ARGS__) \
MACRO(Color, FVector4, __VA_ARGS__) \
MACRO(Steepness, float, __VA_ARGS__) \
MACRO(Seed, int32, __VA_ARGS__) \
MACRO(MetadataEntry, int64, __VA_ARGS__)

#define PCGEX_NATIVE_PROPERTY_GET(_NAME, _TYPE, _SOURCE) TPCGValueRange<_TYPE> _NAME##ValueRange = _SOURCE->Get##_NAME##ValueRange();
#define PCGEX_FOREACH_POINT_NATIVE_PROPERTY_GET(_SOURCE) PCGEX_FOREACH_POINT_NATIVE_PROPERTY(PCGEX_NATIVE_PROPERTY_GET, _SOURCE)

#define PCGEX_NATIVE_PROPERTY_CONSTGET(_NAME, _TYPE, _SOURCE) TConstPCGValueRange<_TYPE> _NAME##ValueRange = _SOURCE->GetValue##_NAME##ValueRange();
#define PCGEX_FOREACH_POINT_NATIVE_PROPERTY_CONSTGET(_SOURCE) PCGEX_FOREACH_POINT_NATIVE_PROPERTY(PCGEX_NATIVE_PROPERTY_CONSTGET, _SOURCE)

#define PCGEX_FOREACH_POINTPROPERTY(MACRO)\
MACRO(EPCGPointProperties::Density, GetDensity(), float, float) \
MACRO(EPCGPointProperties::BoundsMin, GetBoundsMin(), FVector, FVector) \
MACRO(EPCGPointProperties::BoundsMax, GetBoundsMax(), FVector, FVector) \
MACRO(EPCGPointProperties::Extents, GetExtents(), FVector, FVector) \
MACRO(EPCGPointProperties::Color, GetColor(), FVector4, FVector4) \
MACRO(EPCGPointProperties::Position, GetLocation(), FVector, FTransform) \
MACRO(EPCGPointProperties::Rotation, GetRotation(), FQuat, FTransform) \
MACRO(EPCGPointProperties::Scale, GetScale3D(), FVector, FTransform) \
MACRO(EPCGPointProperties::Transform, GetTransform(), FTransform, FTransform) \
MACRO(EPCGPointProperties::Steepness, GetSteepness(), float, float) \
MACRO(EPCGPointProperties::LocalCenter, GetLocalCenter(), FVector, FVector) \
MACRO(EPCGPointProperties::Seed, GetSeed(), int32, int32)\
MACRO(EPCGPointProperties::LocalSize, GetLocalSize(), FVector, FVector)\
MACRO(EPCGPointProperties::ScaledLocalSize, GetScaledLocalSize(), FVector, FVector)

#define PCGEX_FOREACH_EXTRAPROPERTY(MACRO)\
MACRO(EPCGExtraProperties::Index, int32, int32)

#define PCGEX_VALIDATE_NAME(_NAME) if (!PCGExMetaHelpers::IsWritableAttributeName(_NAME)){	PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Invalid user-defined attribute name for " #_NAME)); return false;	}
#define PCGEX_VALIDATE_NAME_CONDITIONAL(_IF, _NAME) if(_IF){ PCGEX_VALIDATE_NAME(_NAME) }
#define PCGEX_VALIDATE_NAME_CONSUMABLE(_NAME) if (!PCGExMetaHelpers::IsWritableAttributeName(_NAME)){	PCGE_LOG(Error, GraphAndLog, FTEXT("Invalid user-defined attribute name for " #_NAME)); return false;	} Context->AddConsumableAttributeName(_NAME);
#define PCGEX_VALIDATE_NAME_C(_CTX, _NAME) if (!PCGExMetaHelpers::IsWritableAttributeName(_NAME)){	PCGE_LOG_C(Error, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); return false;	}
#define PCGEX_VALIDATE_NAME_C_RET(_CTX, _NAME, _RETURN) if (!PCGExMetaHelpers::IsWritableAttributeName(_NAME)){	PCGE_LOG_C(Error, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); return _RETURN;	}
#define PCGEX_VALIDATE_NAME_CONSUMABLE_C(_CTX, _NAME) if (!PCGExMetaHelpers::IsWritableAttributeName(_NAME)){	PCGE_LOG_C(Error, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); return false;	} _CTX->AddConsumableAttributeName(_NAME);
#define PCGEX_SOFT_VALIDATE_NAME(_BOOL, _NAME, _CTX) if(_BOOL){if (!PCGExMetaHelpers::IsWritableAttributeName(_NAME)){ PCGE_LOG_C(Warning, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); _BOOL = false; } }

#pragma endregion

namespace PCGExMetaHelpers
{
	const FName InvalidName = "INVALID_DATA";

	PCGEXCORE_API bool IsPCGExAttribute(const FString& InStr);
	PCGEXCORE_API bool IsPCGExAttribute(const FName InName);
	PCGEXCORE_API bool IsPCGExAttribute(const FText& InText);

	PCGEXCORE_API FName MakePCGExAttributeName(const FString& Str0);
	PCGEXCORE_API FName MakePCGExAttributeName(const FString& Str0, const FString& Str1);

	PCGEXCORE_API bool IsWritableAttributeName(const FName Name);
	PCGEXCORE_API FString StringTagFromName(const FName Name);
	PCGEXCORE_API bool IsValidStringTag(const FString& Tag);

	PCGEXCORE_API bool TryGetAttributeName(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData, FName& OutName);

	PCGEXCORE_API bool IsDataDomainAttribute(const FName& InName);
	PCGEXCORE_API bool IsDataDomainAttribute(const FString& InName);
	PCGEXCORE_API bool IsDataDomainAttribute(const FPCGAttributePropertyInputSelector& InputSelector);

	PCGEXCORE_API void AppendUniqueSelectorsFromCommaSeparatedList(const FString& InCommaSeparatedString, TArray<FPCGAttributePropertyInputSelector>& OutSelectors);

	PCGEXCORE_API FName GetLongNameFromSelector(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData, const bool bInitialized = true);

	PCGEXCORE_API FPCGAttributeIdentifier GetAttributeIdentifier(const FPCGAttributePropertyInputSelector& InSelector, const UPCGData* InData, const bool bInitialized = true);
	PCGEXCORE_API FPCGAttributeIdentifier GetAttributeIdentifier(const FName InName, const UPCGData* InData);
	PCGEXCORE_API FPCGAttributeIdentifier GetAttributeIdentifier(const FName InName);

	PCGEXCORE_API FPCGAttributePropertyInputSelector GetSelectorFromIdentifier(const FPCGAttributeIdentifier& InIdentifier);

	PCGEXCORE_API bool HasAttribute(const UPCGMetadata* InMetadata, const FPCGAttributeIdentifier& Identifier);

	static bool HasAttribute(const UPCGData* InData, const FPCGAttributeIdentifier& Identifier)
	{
		if (!InData) { return false; }
		return HasAttribute(InData->ConstMetadata(), Identifier);
	}

	template <typename T>
	static const FPCGMetadataAttribute<T>* TryGetConstAttribute(const UPCGMetadata* InMetadata, const FPCGAttributeIdentifier& Identifier)
	{
		if (!InMetadata) { return nullptr; }
		if (!InMetadata->GetConstMetadataDomain(Identifier.MetadataDomain)) { return nullptr; }

		// 'template' spec required for clang on mac, and rider keeps removing it without the comment below.
		// ReSharper disable once CppRedundantTemplateKeyword
		return InMetadata->template GetConstTypedAttribute<T>(Identifier);
	}

	template <typename T>
	static const FPCGMetadataAttribute<T>* TryGetConstAttribute(const UPCGData* InData, const FPCGAttributeIdentifier& Identifier)
	{
		if (!InData) { return nullptr; }
		return TryGetConstAttribute<T>(InData->ConstMetadata(), Identifier);
	}

	template <typename T>
	static FPCGMetadataAttribute<T>* TryGetMutableAttribute(UPCGMetadata* InMetadata, const FPCGAttributeIdentifier& Identifier)
	{
		if (!InMetadata) { return nullptr; }
		if (!InMetadata->GetConstMetadataDomain(Identifier.MetadataDomain)) { return nullptr; }

		// 'template' spec required for clang on mac, and rider keeps removing it without the comment below.
		// ReSharper disable once CppRedundantTemplateKeyword
		return InMetadata->template GetMutableTypedAttribute<T>(Identifier);
	}

	template <typename T>
	static FPCGMetadataAttribute<T>* TryGetMutableAttribute(UPCGData* InData, const FPCGAttributeIdentifier& Identifier)
	{
		if (!InData) { return nullptr; }
		return TryGetMutableAttribute<T>(InData->MutableMetadata(), Identifier);
	}

	constexpr static EPCGMetadataTypes GetPropertyType(const EPCGPointProperties Property)
	{
		switch (Property)
		{
		case EPCGPointProperties::Density:
		case EPCGPointProperties::Steepness: return EPCGMetadataTypes::Float;
		case EPCGPointProperties::BoundsMin:
		case EPCGPointProperties::BoundsMax:
		case EPCGPointProperties::Extents:
		case EPCGPointProperties::Position:
		case EPCGPointProperties::Scale:
		case EPCGPointProperties::LocalCenter:
		case EPCGPointProperties::LocalSize:
		case EPCGPointProperties::ScaledLocalSize: return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::Color: return EPCGMetadataTypes::Vector4;
		case EPCGPointProperties::Rotation: return EPCGMetadataTypes::Quaternion;
		case EPCGPointProperties::Transform: return EPCGMetadataTypes::Transform;
		case EPCGPointProperties::Seed: return EPCGMetadataTypes::Integer32;
		default: return EPCGMetadataTypes::Unknown;
		}
	}

	constexpr static EPCGPointNativeProperties GetPropertyNativeTypes(const EPCGPointProperties Property)
	{
		switch (Property)
		{
		case EPCGPointProperties::Density: return EPCGPointNativeProperties::Density;
		case EPCGPointProperties::BoundsMin: return EPCGPointNativeProperties::BoundsMin;
		case EPCGPointProperties::BoundsMax: return EPCGPointNativeProperties::BoundsMax;
		case EPCGPointProperties::Color: return EPCGPointNativeProperties::Color;
		case EPCGPointProperties::Position: return EPCGPointNativeProperties::Transform;
		case EPCGPointProperties::Rotation: return EPCGPointNativeProperties::Transform;
		case EPCGPointProperties::Scale: return EPCGPointNativeProperties::Transform;
		case EPCGPointProperties::Transform: return EPCGPointNativeProperties::Transform;
		case EPCGPointProperties::Steepness: return EPCGPointNativeProperties::Steepness;
		case EPCGPointProperties::Seed: return EPCGPointNativeProperties::Seed;
		case EPCGPointProperties::Extents:
		case EPCGPointProperties::LocalCenter:
		case EPCGPointProperties::LocalSize: return EPCGPointNativeProperties::BoundsMin | EPCGPointNativeProperties::BoundsMax;
		case EPCGPointProperties::ScaledLocalSize: return EPCGPointNativeProperties::BoundsMin | EPCGPointNativeProperties::BoundsMax | EPCGPointNativeProperties::Transform;
		default: return EPCGPointNativeProperties::None;
		}
	}

	constexpr static EPCGMetadataTypes GetPropertyType(const EPCGExtraProperties Property)
	{
		switch (Property)
		{
		case EPCGExtraProperties::Index: return EPCGMetadataTypes::Integer32;
		default: return EPCGMetadataTypes::Unknown;
		}
	}

	constexpr bool DummyBoolean = bool{};
	constexpr int32 DummyInteger32 = int32{};
	constexpr int64 DummyInteger64 = int64{};
	constexpr float DummyFloat = float{};
	constexpr double DummyDouble = double{};
	const FVector2D DummyVector2 = FVector2D::ZeroVector;
	const FVector DummyVector = FVector::ZeroVector;
	const FVector4 DummyVector4 = FVector4::Zero();
	const FQuat DummyQuaternion = FQuat::Identity;
	const FRotator DummyRotator = FRotator::ZeroRotator;
	const FTransform DummyTransform = FTransform::Identity;
	const FString DummyString = TEXT("");
	const FName DummyName = NAME_None;
	const FSoftClassPath DummySoftClassPath = FSoftClassPath{};
	const FSoftObjectPath DummySoftObjectPath = FSoftObjectPath{};

	template <typename Func>
	static void ExecuteWithRightType(const EPCGMetadataTypes Type, Func&& Callback)
	{
#define PCGEX_EXECUTE_WITH_TYPE(_TYPE, _ID, ...) case EPCGMetadataTypes::_ID : Callback(Dummy##_ID); break;

		switch (Type)
		{
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_EXECUTE_WITH_TYPE)
		default: ;
		}

#undef PCGEX_EXECUTE_WITH_TYPE
	}

	template <typename Func>
	static void ExecuteWithRightType(const int16 Type, Func&& Callback)
	{
#define PCGEX_EXECUTE_WITH_TYPE(_TYPE, _ID, ...) case EPCGMetadataTypes::_ID : Callback(Dummy##_ID); break;

		switch (static_cast<EPCGMetadataTypes>(Type))
		{
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_EXECUTE_WITH_TYPE)
		default: ;
		}

#undef PCGEX_EXECUTE_WITH_TYPE
	}

	PCGEXCORE_API FString GetSelectorDisplayName(const FPCGAttributePropertyInputSelector& InSelector);
}
