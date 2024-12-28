// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Runtime/Launch/Resources/Version.h"

#ifndef PCGEX_MACROS
#define PCGEX_MACROS

#define PCGEX_MAKE_SHARED(_NAME, _CLASS, ...) const TSharedPtr<_CLASS> _NAME = MakeShared<_CLASS>(__VA_ARGS__);
#define PCGEX_ENGINE_VERSION ENGINE_MAJOR_VERSION * 100 + ENGINE_MINOR_VERSION

#define PCGEX_CAN_ONLY_EXECUTE_ON_MAIN_THREAD(_BOOL) virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return _BOOL; }

#pragma region PCGEX MACROS

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

#if PCGEX_ENGINE_VERSION <= 503
enum class EPCGPinStatus : uint8
{
	Normal = 0,
	Required,
	Advanced
};
#endif

#if PCGEX_ENGINE_VERSION <= 503
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
#if PCGEX_ENGINE_VERSION <= 504
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
#else
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
MACRO(EPCGPointProperties::Seed, Seed)\
MACRO(EPCGPointProperties::LocalSize, GetLocalSize())\
MACRO(EPCGPointProperties::ScaledLocalSize, GetScaledLocalSize())
#endif

#define PCGEX_FOREACH_POINTEXTRAPROPERTY(MACRO)\
MACRO(EPCGExtraProperties::Index, MetadataEntry)

#define PCGEX_CLEAN_SP(_NAME) _NAME = nullptr;

#pragma endregion

#define PCGEX_NODE_INFOS(_SHORTNAME, _NAME, _TOOLTIP)\
virtual FName GetDefaultNodeName() const override { return FName(TEXT(#_SHORTNAME)); } \
virtual FName AdditionalTaskName() const override{ return bCacheResult ? FName(FString("* ")+GetDefaultNodeTitle().ToString()) : FName(GetDefaultNodeTitle().ToString()); }\
virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGEx" #_SHORTNAME, "NodeTitle", "PCGEx | " _NAME);} \
virtual FText GetNodeTooltipText() const override{ return NSLOCTEXT("PCGEx" #_SHORTNAME "Tooltip", "NodeTooltip", _TOOLTIP); } \
virtual TArray<FPCGPreConfiguredSettingsInfo> GetPreconfiguredInfo() const override{ return {}; }// return {FPCGPreConfiguredSettingsInfo(0, GetDefaultNodeTitle(), GetNodeTooltipText())};}

#if PCGEX_ENGINE_VERSION <= 503
#define PCGEX_NODE_INFOS_CUSTOM_SUBTITLE_LEAN(_SHORTNAME, _NAME, _TOOLTIP, _TASK_NAME)\
virtual FName GetDefaultNodeName() const override { return FName(TEXT(#_SHORTNAME)); } \
virtual FName AdditionalTaskName() const override{ return _TASK_NAME.IsNone() ? FName(GetDefaultNodeTitle().ToString()) : FName(FString(GetDefaultNodeTitle().ToString() + "\r" + _TASK_NAME.ToString())); }\
virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGEx" #_SHORTNAME, "NodeTitle", "PCGEx | " _NAME);} \
virtual FText GetNodeTooltipText() const override{ return NSLOCTEXT("PCGEx" #_SHORTNAME "Tooltip", "NodeTooltip", _TOOLTIP); } 
#else
#define PCGEX_NODE_INFOS_CUSTOM_SUBTITLE_LEAN(_SHORTNAME, _NAME, _TOOLTIP, _TASK_NAME)\
virtual FName GetDefaultNodeName() const override { return FName(TEXT(#_SHORTNAME)); } \
virtual FName AdditionalTaskName() const override{ return FName(GetDefaultNodeTitle().ToString()); }\
virtual FString GetAdditionalTitleInformation() const override{ FName N = _TASK_NAME; return N.IsNone() ? FString() : N.ToString(); }\
virtual bool HasFlippedTitleLines() const { FName N = _TASK_NAME; return !N.IsNone(); }\
virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGEx" #_SHORTNAME, "NodeTitle", "PCGEx | " _NAME);} \
virtual FText GetNodeTooltipText() const override{ return NSLOCTEXT("PCGEx" #_SHORTNAME "Tooltip", "NodeTooltip", _TOOLTIP); }
#endif

#if PCGEX_ENGINE_VERSION <= 503
#define PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(_SHORTNAME, _NAME, _TOOLTIP, _TASK_NAME)\
PCGEX_NODE_INFOS_CUSTOM_SUBTITLE_LEAN(_SHORTNAME, _NAME, _TOOLTIP, _TASK_NAME)\
virtual TArray<FPCGPreConfiguredSettingsInfo> GetPreconfiguredInfo() const override{ return {}; } // return {FPCGPreConfiguredSettingsInfo(0, GetDefaultNodeTitle(), GetNodeTooltipText())};}
#else
#define PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(_SHORTNAME, _NAME, _TOOLTIP, _TASK_NAME)\
PCGEX_NODE_INFOS_CUSTOM_SUBTITLE_LEAN(_SHORTNAME, _NAME, _TOOLTIP, _TASK_NAME)\
virtual TArray<FPCGPreConfiguredSettingsInfo> GetPreconfiguredInfo() const override{ return {}; } // return {FPCGPreConfiguredSettingsInfo(0, GetDefaultNodeTitle(), GetNodeTooltipText())};}
#endif

#define PCGEX_NODE_POINT_FILTER(_LABEL, _TOOLTIP, _TYPE, _REQUIRED) \
virtual FName GetPointFilterPin() const override { return _LABEL; } \
virtual FString GetPointFilterTooltip() const override { return TEXT(_TOOLTIP); } \
virtual TSet<PCGExFactories::EType> GetPointFilterTypes() const override { return _TYPE; } \
virtual bool RequiresPointFilters() const override { return _REQUIRED; }

#define PCGEX_INITIALIZE_CONTEXT(_NAME)\
FPCGContext* FPCGEx##_NAME##Element::Initialize( const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)\
{	FPCGEx##_NAME##Context* Context = new FPCGEx##_NAME##Context();	return InitializeContext(Context, InputData, SourceComponent, Node); }

#define PCGEX_INITIALIZE_ELEMENT(_NAME)\
PCGEX_INITIALIZE_CONTEXT(_NAME)\
FPCGElementPtr UPCGEx##_NAME##Settings::CreateElement() const{	return MakeShared<FPCGEx##_NAME##Element>();}
#define PCGEX_CONTEXT(_NAME) FPCGEx##_NAME##Context* Context = static_cast<FPCGEx##_NAME##Context*>(InContext);
#define PCGEX_SETTINGS(_NAME) const UPCGEx##_NAME##Settings* Settings = Context->GetInputSettings<UPCGEx##_NAME##Settings>();	check(Settings);
#define PCGEX_SETTINGS_LOCAL(_NAME) const UPCGEx##_NAME##Settings* Settings = GetInputSettings<UPCGEx##_NAME##Settings>();	check(Settings);
#define PCGEX_CONTEXT_AND_SETTINGS(_NAME) PCGEX_CONTEXT(_NAME) PCGEX_SETTINGS(_NAME)
#define PCGEX_OPERATION_VALIDATE(_NAME) if(!Settings->_NAME){PCGE_LOG(Error, GraphAndLog, FTEXT("No operation selected for : "#_NAME)); return false;}
#define PCGEX_OPERATION_BIND(_NAME, _TYPE, _OVERRIDES_PIN) PCGEX_OPERATION_VALIDATE(_NAME) Context->_NAME = Context->RegisterOperation<_TYPE>(Settings->_NAME, _OVERRIDES_PIN );
#define PCGEX_VALIDATE_NAME(_NAME) if (!PCGEx::IsValidName(_NAME)){	PCGE_LOG(Error, GraphAndLog, FTEXT("Invalid user-defined attribute name for " #_NAME)); return false;	}
#define PCGEX_VALIDATE_NAME_C(_CTX, _NAME) if (!PCGEx::IsValidName(_NAME)){	PCGE_LOG_C(Error, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); return false;	}
#define PCGEX_SOFT_VALIDATE_NAME(_BOOL, _NAME, _CTX) if(_BOOL){if (!PCGEx::IsValidName(_NAME)){ PCGE_LOG_C(Warning, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); _BOOL = false; } }
#define PCGEX_FWD(_NAME) Context->_NAME = Settings->_NAME;
#define PCGEX_TERMINATE_ASYNC Lifecycle->Terminate(); if (AsyncManager) { AsyncManager->Stop(); }

#define PCGEX_TYPED_CONTEXT_AND_SETTINGS(_NAME) FPCGEx##_NAME##Context* Context = static_cast<FPCGEx##_NAME##Context*>(ExecutionContext); const UPCGEx##_NAME##Settings* Settings = Context->GetInputSettings<UPCGEx##_NAME##Settings>(); check(Settings);

#if WITH_EDITOR
#define PCGEX_PIN_TOOLTIP(_TOOLTIP) Pin.Tooltip = FTEXT(_TOOLTIP);
#else
#define PCGEX_PIN_TOOLTIP(_TOOLTIP) {}
#endif

#if PCGEX_ENGINE_VERSION > 503
#define PCGEX_PIN_STATUS(_STATUS) Pin.PinStatus = EPCGPinStatus::_STATUS;
#else
#define PCGEX_PIN_STATUS(_STATUS) Pin.bAdvancedPin = EPCGPinStatus::_STATUS == EPCGPinStatus::Advanced;
#endif

#define PCGEX_PIN_ANY(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Any); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_POINTS(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Point); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_POLYLINES(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::PolyLine); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_PARAMS(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Param); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_TEXTURES(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::BaseTexture); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_POINT(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Point, false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_PARAM(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Param, false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_TEXTURE(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::BaseTexture, false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_OPERATION_OVERRIDES(_LABEL) PCGEX_PIN_PARAMS(_LABEL, "Property overrides to be forwarded & processed by the module. Name must match the property you're targeting 1:1, type mismatch will be broadcasted at your own risk.", Advanced, {})
#define PCGEX_PIN_DEPENDENCIES PCGEX_PIN_ANY(PCGPinConstants::DefaultDependencyOnlyLabel, "Data passed to this pin will be used to order execution but will otherwise not contribute to the results of this node.", Normal, {})

#define PCGEX_OCTREE_SEMANTICS(_ITEM, _BOUNDS, _EQUALITY)\
struct _ITEM##Semantics{ \
	enum { MaxElementsPerLeaf = 16 }; \
	enum { MinInclusiveElementsPerNode = 7 }; \
	enum { MaxNodeDepth = 12 }; \
	using ElementAllocator = TInlineAllocator<MaxElementsPerLeaf>; \
	FORCEINLINE static const FBoxSphereBounds& GetBoundingBox(const _ITEM* Element) _BOUNDS \
	FORCEINLINE static const bool AreElementsEqual(const _ITEM* A, const _ITEM* B) _EQUALITY \
	FORCEINLINE static void ApplyOffset(_ITEM& Element){ ensureMsgf(false, TEXT("Not implemented")); } \
	FORCEINLINE static void SetElementId(const _ITEM* Element, FOctreeElementId2 OctreeElementID){ }}; \
	using _ITEM##Octree = TOctree2<_ITEM*, _ITEM##Semantics>;

#define PCGEX_OCTREE_SEMANTICS_REF(_ITEM, _BOUNDS, _EQUALITY)\
struct _ITEM##Semantics{ \
enum { MaxElementsPerLeaf = 16 }; \
enum { MinInclusiveElementsPerNode = 7 }; \
enum { MaxNodeDepth = 12 }; \
using ElementAllocator = TInlineAllocator<MaxElementsPerLeaf>; \
FORCEINLINE static const FBoxSphereBounds& GetBoundingBox(const _ITEM& Element) _BOUNDS \
FORCEINLINE static const bool AreElementsEqual(const _ITEM& A, const _ITEM& B) _EQUALITY \
FORCEINLINE static void ApplyOffset(_ITEM& Element){ ensureMsgf(false, TEXT("Not implemented")); } \
FORCEINLINE static void SetElementId(const _ITEM& Element, FOctreeElementId2 OctreeElementID){ }}; \
using _ITEM##Octree = TOctree2<_ITEM, _ITEM##Semantics>;

#endif
