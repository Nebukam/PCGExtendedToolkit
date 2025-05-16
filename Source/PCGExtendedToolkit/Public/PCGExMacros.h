// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Runtime/Launch/Resources/Version.h"

#ifndef PCGEX_MACROS
#define PCGEX_MACROS

#define PCGEX_MAKE_SHARED(_NAME, _CLASS, ...) const TSharedPtr<_CLASS> _NAME = MakeShared<_CLASS>(__VA_ARGS__);
#define PCGEX_ENGINE_VERSION ENGINE_MAJOR_VERSION * 100 + ENGINE_MINOR_VERSION

#define PCGEX_CAN_ONLY_EXECUTE_ON_MAIN_THREAD(_BOOL) virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return _BOOL; }

#pragma region PCGEX MACROS

#define PCGEX_LOG_INVALID_SELECTOR_C(_CTX, _NAME, _SELECTOR) PCGE_LOG_C(Error, GraphAndLog, _CTX, FText::Format(FTEXT("Invalid "#_NAME" attribute: \"{0}\"."), FText::FromString(PCGEx::GetSelectorDisplayName(_SELECTOR))));
#define PCGEX_LOG_INVALID_ATTR_C(_CTX, _NAME, _SELECTOR) PCGE_LOG_C(Error, GraphAndLog, _CTX, FText::Format(FTEXT("Invalid "#_NAME" attribute: \"{0}\"."), FText::FromName(_SELECTOR)));

#define PCGEX_ON_INVALILD_INPUTS(_MSG) bool bHasInvalidInputs = false; ON_SCOPE_EXIT{ if (bHasInvalidInputs){ PCGE_LOG(Warning, GraphAndLog, _MSG); } };

#define PCGEX_NOT_IMPLEMENTED(_NAME){ LowLevelFatalError(TEXT("Method not implemented: (%s)"), TEXT(#_NAME));}
#define PCGEX_NOT_IMPLEMENTED_RET(_NAME, _RETURN){ LowLevelFatalError(TEXT("Method not implemented: (%s)"), TEXT(#_NAME)); return _RETURN;}

#define PCGEX_LOG_CTR(_NAME)  //UE_LOG(LogTemp, Warning, TEXT(#_NAME"::Constructor"))
#define PCGEX_LOG_DTR(_NAME)  //UE_LOG(LogTemp, Warning, TEXT(#_NAME"::Destructor"))

#define FTEXT(_TEXT) FText::FromString(FString(_TEXT))
#define FSTRING(_TEXT) FString(_TEXT)

#define PCGEX_FOREACH_XYZ(MACRO)\
MACRO(X)\
MACRO(Y)\
MACRO(Z)

#define PCGEX_CONSUMABLE_SELECTOR(_SELECTOR, _NAME) if (PCGExHelpers::TryGetAttributeName(_SELECTOR, InData, _NAME)) { InContext->AddConsumableAttributeName(_NAME); }
#define PCGEX_CONSUMABLE_SELECTOR_C(_CONTEXT, _SELECTOR, _NAME) if (PCGExHelpers::TryGetAttributeName(_SELECTOR, InData, _NAME)) { _CONTEXT->AddConsumableAttributeName(_NAME); }
#define PCGEX_CONSUMABLE_CONDITIONAL(_CONDITION, _SELECTOR, _NAME) if (_CONDITION && PCGExHelpers::TryGetAttributeName(_SELECTOR, InData, _NAME)) { InContext->AddConsumableAttributeName(_NAME); }

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

/**
 * Enum, Point.[Getter]
 * @param MACRO 
 */

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

#define PCGEX_NATIVE_PROPERTY_CONSTGET(_NAME, _TYPE, _SOURCE) TConstPCGValueRange<_TYPE> _NAME##ValueRange = _SOURCE->GetConst##_NAME##ValueRange();
#define PCGEX_FOREACH_POINT_NATIVE_PROPERTY_CONSTGET(_SOURCE) PCGEX_FOREACH_POINT_NATIVE_PROPERTY(PCGEX_NATIVE_PROPERTY_CONSTGET, _SOURCE)

#define PCGEX_FOREACH_POINTPROPERTY(MACRO)\
MACRO(EPCGPointProperties::Density, Density, float) \
MACRO(EPCGPointProperties::BoundsMin, BoundsMin, FVector) \
MACRO(EPCGPointProperties::BoundsMax, BoundsMax, FVector) \
MACRO(EPCGPointProperties::Extents, GetExtents(), FVector) \
MACRO(EPCGPointProperties::Color, Color, FVector4) \
MACRO(EPCGPointProperties::Position, Transform.GetLocation(), FVector) \
MACRO(EPCGPointProperties::Rotation, Transform.GetRotation(), FQuat) \
MACRO(EPCGPointProperties::Scale, Transform.GetScale3D(), FVector) \
MACRO(EPCGPointProperties::Transform, Transform, FTransform) \
MACRO(EPCGPointProperties::Steepness, Steepness, float) \
MACRO(EPCGPointProperties::LocalCenter, GetLocalCenter(), FVector) \
MACRO(EPCGPointProperties::Seed, Seed, int32)\
MACRO(EPCGPointProperties::LocalSize, GetLocalSize(), FVector)\
MACRO(EPCGPointProperties::ScaledLocalSize, GetScaledLocalSize(), FVector)

#define PCGEX_CONSTEXPR_IFELSE_GETPOINTPROPERTY(_PROPERTY, MACRO)\
if constexpr(_PROPERTY == EPCGPointProperties::Density){ MACRO(Density, float) } \
else if constexpr(_PROPERTY == EPCGPointProperties::BoundsMin){ MACRO(BoundsMin, FVector) } \
else if constexpr(_PROPERTY == EPCGPointProperties::BoundsMax){ MACRO(BoundsMax, FVector) } \
else if constexpr(_PROPERTY == EPCGPointProperties::Extents){ MACRO(GetExtents(), FVector) } \
else if constexpr(_PROPERTY == EPCGPointProperties::Color){ MACRO(Color, FVector4) } \
else if constexpr(_PROPERTY == EPCGPointProperties::Position){ MACRO(Transform.GetLocation(), FVector) } \
else if constexpr(_PROPERTY == EPCGPointProperties::Rotation){ MACRO(Transform.GetRotation(), FQuat) } \
else if constexpr(_PROPERTY == EPCGPointProperties::Scale){ MACRO(Transform.GetScale3D(), FVector) } \
else if constexpr(_PROPERTY == EPCGPointProperties::Transform){ MACRO(Transform, FTransform) } \
else if constexpr(_PROPERTY == EPCGPointProperties::Steepness){ MACRO(Steepness, float) } \
else if constexpr(_PROPERTY == EPCGPointProperties::LocalCenter){ MACRO(GetLocalCenter(), FVector) } \
else if constexpr(_PROPERTY == EPCGPointProperties::Seed){ MACRO(Seed, int32) } \
else if constexpr(_PROPERTY == EPCGPointProperties::LocalSize){ MACRO(GetLocalSize(), FVector) } \
else if constexpr(_PROPERTY == EPCGPointProperties::ScaledLocalSize){ MACRO(GetScaledLocalSize(), FVector) }

#define PCGEX_MACRO_NONE(...)

#define PCGEX_CONSTEXPR_IFELSE_SETPOINTPROPERTY(_PROPERTY, _POINT, BODY, MACRO)\
if constexpr(_PROPERTY == EPCGPointProperties::Density){ BODY(FVector) _POINT.Density = MACRO(float); } \
else if constexpr(_PROPERTY == EPCGPointProperties::BoundsMin){ BODY(FVector) _POINT.BoundsMin = MACRO(FVector); } \
else if constexpr(_PROPERTY == EPCGPointProperties::BoundsMax){ BODY(FVector) _POINT.BoundsMax = MACRO(FVector); } \
else if constexpr(_PROPERTY == EPCGPointProperties::Extents){ BODY(FVector) _POINT.SetExtents(MACRO(FVector)); } \
else if constexpr(_PROPERTY == EPCGPointProperties::Color){ BODY(FVector) _POINT.Color = MACRO(FVector4); } \
else if constexpr(_PROPERTY == EPCGPointProperties::Position){ BODY(FVector) _POINT.Transform.SetLocation(MACRO(FVector)); } \
else if constexpr(_PROPERTY == EPCGPointProperties::Rotation){ BODY(FVector) _POINT.Transform.SetRotation(MACRO(FQuat)); } \
else if constexpr(_PROPERTY == EPCGPointProperties::Scale){ BODY(FVector) _POINT.Transform.SetScale3D(MACRO(FVector)); } \
else if constexpr(_PROPERTY == EPCGPointProperties::Transform){ BODY(FVector) _POINT.Transform = MACRO(FTransform); } \
else if constexpr(_PROPERTY == EPCGPointProperties::Steepness){ BODY(FVector) _POINT.Steepness = MACRO(float); } \
else if constexpr(_PROPERTY == EPCGPointProperties::LocalCenter){ BODY(FVector) _POINT.SetLocalCenter(MACRO(FVector)); } \
else if constexpr(_PROPERTY == EPCGPointProperties::Seed){ BODY(FVector) _POINT.Seed = MACRO(int32); } \
else if constexpr(_PROPERTY == EPCGPointProperties::LocalSize){ BODY(FVector) _POINT.SetExtents((MACRO(FVector))*0.5);  } \
else if constexpr(_PROPERTY == EPCGPointProperties::ScaledLocalSize){ BODY(FVector) _POINT.SetExtents((MACRO(FVector)) * (FVector::OneVector / _POINT.Transform.GetScale3D()) * 0.5); }

#pragma endregion

// Dummy members required by Macros (I know! Don't say it!)
#define PCGEX_DUMMY_SETTINGS_MEMBERS \
bool ShouldCache() const { return false; } \
bool bCleanupConsumableAttributes = false;

// FString A = ShouldCache() ? TEXT("♻️ ") : TEXT("");

#define PCGEX_NODE_INFOS(_SHORTNAME, _NAME, _TOOLTIP)\
virtual FName GetDefaultNodeName() const override { return FName(TEXT("PCGEx"#_SHORTNAME)); } \
virtual FName AdditionalTaskName() const override{ FString A = TEXT(""); return FName(A + GetDefaultNodeTitle().ToString()); }\
virtual FText GetDefaultNodeTitle() const override { FString A = TEXT(""); A += TEXT("PCGEx | "); A += (bCleanupConsumableAttributes ? TEXT("🗑️ ") : TEXT("")); A += TEXT(_NAME); return FTEXT(A);} \
virtual FText GetNodeTooltipText() const override{ return FTEXT(_TOOLTIP); }

#define PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(_SHORTNAME, _NAME, _TOOLTIP, _TASK_NAME)\
virtual FName GetDefaultNodeName() const override { return FName(TEXT(#_SHORTNAME)); } \
virtual FName AdditionalTaskName() const override{ FString A = TEXT(""); return FName(A + GetDefaultNodeTitle().ToString()); }\
virtual FString GetAdditionalTitleInformation() const override{ FName N = _TASK_NAME; return N.IsNone() ? FString() : N.ToString(); }\
virtual bool HasFlippedTitleLines() const { FName N = _TASK_NAME; return !N.IsNone(); }\
virtual FText GetDefaultNodeTitle() const override { FString A = TEXT(""); A += TEXT("PCGEx | ");  A += (bCleanupConsumableAttributes ? TEXT("🗑️ ") : TEXT("")); A += TEXT(_NAME); return FTEXT(A);} \
virtual FText GetNodeTooltipText() const override{ return FTEXT(_TOOLTIP); }

#define PCGEX_NODE_POINT_FILTER(_LABEL, _TOOLTIP, _TYPE, _REQUIRED) \
virtual FName GetPointFilterPin() const override { return _LABEL; } \
virtual FString GetPointFilterTooltip() const override { return TEXT(_TOOLTIP); } \
virtual TSet<PCGExFactories::EType> GetPointFilterTypes() const override { return _TYPE; } \
virtual bool RequiresPointFilters() const override { return _REQUIRED; }

#define PCGEX_INITIALIZE_ELEMENT(_NAME)\
FPCGElementPtr UPCGEx##_NAME##Settings::CreateElement() const{	return MakeShared<FPCGEx##_NAME##Element>();}
#define PCGEX_CONTEXT(_NAME) FPCGEx##_NAME##Context* Context = static_cast<FPCGEx##_NAME##Context*>(InContext);
#define PCGEX_SETTINGS(_NAME) const UPCGEx##_NAME##Settings* Settings = Context->GetInputSettings<UPCGEx##_NAME##Settings>();	check(Settings);
#define PCGEX_SETTINGS_LOCAL(_NAME) const UPCGEx##_NAME##Settings* Settings = GetInputSettings<UPCGEx##_NAME##Settings>();	check(Settings);
#define PCGEX_CONTEXT_AND_SETTINGS(_NAME) PCGEX_CONTEXT(_NAME) PCGEX_SETTINGS(_NAME)
#define PCGEX_OPERATION_VALIDATE(_NAME) if(!Settings->_NAME){PCGE_LOG(Error, GraphAndLog, FTEXT("No operation selected for : "#_NAME)); return false;}
#define PCGEX_OPERATION_BIND(_NAME, _TYPE, _OVERRIDES_PIN) PCGEX_OPERATION_VALIDATE(_NAME) Context->_NAME = Context->RegisterOperation<_TYPE>(Settings->_NAME, _OVERRIDES_PIN );
#define PCGEX_VALIDATE_NAME(_NAME) if (!PCGEx::IsValidName(_NAME)){	PCGE_LOG(Error, GraphAndLog, FTEXT("Invalid user-defined attribute name for " #_NAME)); return false;	}
#define PCGEX_VALIDATE_NAME_CONSUMABLE(_NAME) if (!PCGEx::IsValidName(_NAME)){	PCGE_LOG(Error, GraphAndLog, FTEXT("Invalid user-defined attribute name for " #_NAME)); return false;	} Context->AddConsumableAttributeName(_NAME);
#define PCGEX_VALIDATE_NAME_C(_CTX, _NAME) if (!PCGEx::IsValidName(_NAME)){	PCGE_LOG_C(Error, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); return false;	}
#define PCGEX_VALIDATE_NAME_C_RET(_CTX, _NAME, _RETURN) if (!PCGEx::IsValidName(_NAME)){	PCGE_LOG_C(Error, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); return _RETURN;	}
#define PCGEX_VALIDATE_NAME_CONSUMABLE_C(_CTX, _NAME) if (!PCGEx::IsValidName(_NAME)){	PCGE_LOG_C(Error, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); return false;	} _CTX->AddConsumableAttributeName(_NAME);
#define PCGEX_SOFT_VALIDATE_NAME(_BOOL, _NAME, _CTX) if(_BOOL){if (!PCGEx::IsValidName(_NAME)){ PCGE_LOG_C(Warning, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); _BOOL = false; } }
#define PCGEX_FWD(_NAME) Context->_NAME = Settings->_NAME;
#define PCGEX_TERMINATE_ASYNC WorkPermit.Reset(); if (AsyncManager) { AsyncManager->Cancel(); }

#define PCGEX_CHECK_WORK_PERMIT_VOID if(!WorkPermit.IsValid()){return;}
#define PCGEX_CHECK_WORK_PERMIT_OR_VOID(_OR) if(!WorkPermit.IsValid() || _OR){return;}
#define PCGEX_CHECK_WORK_PERMIT(_RET) if(!WorkPermit.IsValid()){return _RET;}
#define PCGEX_CHECK_WORK_PERMIT_OR(_OR, _RET) if(!WorkPermit.IsValid() || _OR){return _RET;}

#define PCGEX_GET_OPTION_STATE(_OPTION, _DEFAULT)\
switch (_OPTION){ \
default: case EPCGExOptionState::Default: return GetDefault<UPCGExGlobalSettings>()->_DEFAULT; \
case EPCGExOptionState::Enabled: return true; \
case EPCGExOptionState::Disabled: return false; }

#define PCGEX_TYPED_CONTEXT_AND_SETTINGS(_NAME) FPCGEx##_NAME##Context* Context = static_cast<FPCGEx##_NAME##Context*>(ExecutionContext); const UPCGEx##_NAME##Settings* Settings = Context->GetInputSettings<UPCGEx##_NAME##Settings>(); check(Settings);

#if WITH_EDITOR
#define PCGEX_PIN_TOOLTIP(_TOOLTIP) Pin.Tooltip = FTEXT(_TOOLTIP);
#else
#define PCGEX_PIN_TOOLTIP(_TOOLTIP) {}
#endif

#define PCGEX_PIN_STATUS(_STATUS) Pin.PinStatus = EPCGPinStatus::_STATUS;

#define PCGEX_PIN_ANY(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Any); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_POINTS(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Point); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_POLYLINES(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::PolyLine); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_PARAMS(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Param); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_FACTORIES(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Param); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_TEXTURES(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::BaseTexture); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_ANY_SINGLE(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Any, false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_POINT(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Point, false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_PARAM(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Param, false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_FACTORY(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Param, false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_TEXTURE(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::BaseTexture, false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_OPERATION_OVERRIDES(_LABEL) PCGEX_PIN_PARAMS(_LABEL, "Property overrides to be forwarded & processed by the module. Name must match the property you're targeting 1:1, type mismatch will be broadcasted at your own risk.", Advanced, {})
#define PCGEX_PIN_DEPENDENCIES PCGEX_PIN_ANY(PCGPinConstants::DefaultExecutionDependencyLabel, "Data passed to this pin will be used to order execution but will otherwise not contribute to the results of this node.", Normal, {})

#define PCGEX_OCTREE_SEMANTICS(_ITEM, _BOUNDS, _EQUALITY)\
struct _ITEM##Semantics{ \
	enum { MaxElementsPerLeaf = 16 }; \
	enum { MinInclusiveElementsPerNode = 7 }; \
	enum { MaxNodeDepth = 12 }; \
	using ElementAllocator = TInlineAllocator<MaxElementsPerLeaf>; \
	static const FBoxSphereBounds& GetBoundingBox(const _ITEM* Element) _BOUNDS \
	static const bool AreElementsEqual(const _ITEM* A, const _ITEM* B) _EQUALITY \
	static void ApplyOffset(_ITEM& Element){ ensureMsgf(false, TEXT("Not implemented")); } \
	static void SetElementId(const _ITEM* Element, FOctreeElementId2 OctreeElementID){ }}; \
	using _ITEM##Octree = TOctree2<_ITEM*, _ITEM##Semantics>;

#define PCGEX_OCTREE_SEMANTICS_REF(_ITEM, _BOUNDS, _EQUALITY)\
struct PCGEXTENDEDTOOLKIT_API _ITEM##Semantics{ \
enum { MaxElementsPerLeaf = 16 }; \
enum { MinInclusiveElementsPerNode = 7 }; \
enum { MaxNodeDepth = 12 }; \
using ElementAllocator = TInlineAllocator<MaxElementsPerLeaf>; \
static const FBoxSphereBounds& GetBoundingBox(const _ITEM& Element) _BOUNDS \
static const bool AreElementsEqual(const _ITEM& A, const _ITEM& B) _EQUALITY \
static void ApplyOffset(_ITEM& Element){ ensureMsgf(false, TEXT("Not implemented")); } \
static void SetElementId(const _ITEM& Element, FOctreeElementId2 OctreeElementID){ }}; \
using _ITEM##Octree = TOctree2<_ITEM, _ITEM##Semantics>;

#define PCGEX_NEW_POINT_DATA_TYPE UPCGBasePointData

#define PCGEX_REDUCE_INDICES(_OUT, _NUM, _CONDITION)\
TArray<int32> _OUT; {\
	int32 ElementIndex = 0;\
	int32 NumElements = _NUM;\
	_OUT.SetNumUninitialized(NumElements);\
	for (int32 i = 0; i < NumElements; i++) { if (_CONDITION) { _OUT[ElementIndex++] = i; } }\
	_OUT.SetNum(ElementIndex);\
}

#endif
