// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGExVersion.h"

#ifndef PCGEX_MACROS
#define PCGEX_MACROS

#define PCGEX_MACRO_NONE(...)

#define PCGEX_MAKE_SHARED(_NAME, _CLASS, ...) const TSharedPtr<_CLASS> _NAME = MakeShared<_CLASS>(__VA_ARGS__);

#define PCGEX_CAN_ONLY_EXECUTE_ON_MAIN_THREAD(_BOOL) virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return _BOOL; }
#define PCGEX_SUPPORT_BASE_POINT_DATA(_BOOL) virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return _BOOL; }

#pragma region PCGEX MACROS

#define PCGEX_SHARED_TCONTEXT_VOID(_TYPE, _HANDLE) \
FPCGContext::FSharedContext<FPCGEx##_TYPE##Context> SharedContext(_HANDLE); \
if(!SharedContext.Get()){ return; }

#define PCGEX_SHARED_TCONTEXT(_TYPE, _HANDLE) \
FPCGContext::FSharedContext<FPCGEx##_TYPE##Context> SharedContext(_HANDLE); \
if(!SharedContext.Get()){ return false; }

#define PCGEX_SHARED_CONTEXT_VOID(_HANDLE) PCGEX_SHARED_TCONTEXT_VOID(, _HANDLE)
#define PCGEX_SHARED_CONTEXT(_HANDLE) PCGEX_SHARED_TCONTEXT(, _HANDLE)

#define PCGEX_SHARED_CONTEXT_RET(_HANDLE, _RET) \
FPCGContext::FSharedContext<FPCGExContext> SharedContext(_HANDLE); \
if(!SharedContext.Get()){ return _RET; }

#define PCGEX_LOG_MISSING_INPUT(_CTX, _MESSAGE) if (_CTX && !_CTX->bQuietMissingInputError) { PCGE_LOG_C(Error, GraphAndLog, _CTX, _MESSAGE); }
#define PCGEX_LOG_INVALID_INPUT(_CTX, _MESSAGE) if (_CTX && !_CTX->bQuietInvalidInputWarning) { PCGE_LOG_C(Warning, GraphAndLog, _CTX, _MESSAGE); }

#define PCGEX_LOG_INVALID_SELECTOR_C(_CTX, _NAME, _SELECTOR) if(_CTX && !_CTX->bQuietMissingAttributeError){ PCGE_LOG_C(Error, GraphAndLog, _CTX, FText::Format(FTEXT("Attribute or property \"{0}\" doesn't exist. (See "#_NAME")"), FText::FromString(PCGExMetaHelpers::GetSelectorDisplayName(_SELECTOR)))); }
#define PCGEX_LOG_INVALID_ATTR_C(_CTX, _NAME, _ATTR) if(_CTX && !_CTX->bQuietMissingAttributeError){ PCGE_LOG_C(Error, GraphAndLog, _CTX, FText::Format(FTEXT("Attribute \"{0}\" doesn't exist. (See "#_NAME")"), FText::FromName(_ATTR))); }

#define PCGEX_QUIET_HANDLING_RET return Factory->InitializationFailurePolicy == EPCGExFilterNoDataFallback::Pass;
#define PCGEX_QUIET_HANDLING (Factory->InitializationFailurePolicy != EPCGExFilterNoDataFallback::Error)
#define PCGEX_LOG_INVALID_SELECTOR_HANDLED_C(_CTX, _NAME, _SELECTOR) if(!PCGEX_QUIET_HANDLING){ PCGEX_LOG_INVALID_SELECTOR_C(_CTX, _NAME, _SELECTOR) }
#define PCGEX_LOG_INVALID_ATTR_HANDLED_C(_CTX, _NAME, _ATTR) if(!PCGEX_QUIET_HANDLING){ PCGEX_LOG_INVALID_ATTR_C(_CTX, _NAME, _ATTR) }

#define PCGEX_LOG_WARN_SELECTOR_C(_CTX, _NAME, _SELECTOR) if(_CTX && !_CTX->bQuietInvalidInputWarning){ PCGE_LOG_C(Warning, GraphAndLog, _CTX, FText::Format(FTEXT("Attribute or property \"{0}\" doesn't exist. (See "#_NAME")"), FText::FromString(PCGExMetaHelpers::GetSelectorDisplayName(_SELECTOR)))); }
#define PCGEX_LOG_WARN_ATTR_C(_CTX, _NAME, _ATTR) if(_CTX && !_CTX->bQuietInvalidInputWarning){ PCGE_LOG_C(Warning, GraphAndLog, _CTX, FText::Format(FTEXT("Attribute \"{0}\" doesn't exist. (See "#_NAME")"), FText::FromName(_ATTR))); }

#define PCGEX_ON_INVALILD_INPUTS(_MSG) bool bHasInvalidInputs = false; ON_SCOPE_EXIT{ if (bHasInvalidInputs){ PCGEX_LOG_INVALID_INPUT(Context, _MSG); } };
#define PCGEX_ON_INVALILD_INPUTS_C(_CTX, _MSG) bool bHasInvalidInputs = false; ON_SCOPE_EXIT{ if (bHasInvalidInputs){ PCGEX_LOG_INVALID_INPUT(_CTX, _MSG); } };

#define PCGEX_NOT_IMPLEMENTED(_NAME){ LowLevelFatalError(TEXT("Method not implemented: (%s)"), TEXT(#_NAME));}
#define PCGEX_NOT_IMPLEMENTED_RET(_NAME, _RETURN){ LowLevelFatalError(TEXT("Method not implemented: (%s)"), TEXT(#_NAME)); return _RETURN;}

#define FTEXT(_TEXT) FText::FromString(FString(_TEXT))

#define PCGEX_CONSUMABLE_SELECTOR(_SELECTOR, _NAME) if (PCGExMetaHelpers::TryGetAttributeName(_SELECTOR, InData, _NAME)) { InContext->AddConsumableAttributeName(_NAME); }
#define PCGEX_CONSUMABLE_SELECTOR_C(_CONTEXT, _SELECTOR, _NAME) if (PCGExMetaHelpers::TryGetAttributeName(_SELECTOR, InData, _NAME)) { _CONTEXT->AddConsumableAttributeName(_NAME); }
#define PCGEX_CONSUMABLE_CONDITIONAL(_CONDITION, _SELECTOR, _NAME) if (_CONDITION && PCGExMetaHelpers::TryGetAttributeName(_SELECTOR, InData, _NAME)) { InContext->AddConsumableAttributeName(_NAME); }


#pragma endregion

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
#define PCGEX_CONTEXT(_NAME) check(InContext) FPCGEx##_NAME##Context* Context = static_cast<FPCGEx##_NAME##Context*>(InContext); check(Context);
#define PCGEX_SETTINGS(_NAME) const UPCGEx##_NAME##Settings* Settings = Context->GetInputSettings<UPCGEx##_NAME##Settings>();	check(Settings);
#define PCGEX_SETTINGS_C(_CTX, _NAME) const UPCGEx##_NAME##Settings* Settings = _CTX->GetInputSettings<UPCGEx##_NAME##Settings>();	check(Settings);
#define PCGEX_SETTINGS_LOCAL(_NAME) const UPCGEx##_NAME##Settings* Settings = GetInputSettings<UPCGEx##_NAME##Settings>();	check(Settings);
#define PCGEX_CONTEXT_AND_SETTINGS(_NAME) PCGEX_CONTEXT(_NAME) PCGEX_SETTINGS(_NAME)
#define PCGEX_OPERATION_VALIDATE(_NAME) if(!Settings->_NAME){PCGE_LOG(Error, GraphAndLog, FTEXT("No operation selected for : "#_NAME)); return false;}
#define PCGEX_OPERATION_REGISTER_C(_CTX, _TYPE, _OP, _OVERRIDES_PIN) Cast<_TYPE>(_CTX->RegisterOperation(_OP, _OVERRIDES_PIN))
#define PCGEX_OPERATION_BIND(_NAME, _TYPE, _OVERRIDES_PIN) PCGEX_OPERATION_VALIDATE(_NAME) Context->_NAME = PCGEX_OPERATION_REGISTER_C(Context, _TYPE, Settings->_NAME, _OVERRIDES_PIN); if(!Context->_NAME){return false;}

#define PCGEX_FWD(_NAME) Context->_NAME = Settings->_NAME;
#define PCGEX_TERMINATE_ASYNC WorkHandle.Reset(); if (TaskManager) { TaskManager->Cancel(); }

#define PCGEX_CHECK_WORK_HANDLE_VOID if(!WorkHandle.IsValid()){return;}
#define PCGEX_CHECK_WORK_HANDLE_OR_VOID(_OR) if(!WorkHandle.IsValid() || _OR){return;}
#define PCGEX_CHECK_WORK_HANDLE(_RET) if(!WorkHandle.IsValid()){return _RET;}
#define PCGEX_CHECK_WORK_HANDLE_OR(_OR, _RET) if(!WorkHandle.IsValid() || _OR){return _RET;}

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

#if PCGEX_ENGINE_VERSION < 507
#define PCGEX_PIN_ANY(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Any); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_POINTS(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Point); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_SPATIALS(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Spatial); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_POLYLINES(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::PolyLine); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_MESH(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::DynamicMesh); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_PARAMS(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Param); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS)  }
#define PCGEX_PIN_FILTERS(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Param); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_FACTORIES(_LABEL, _TOOLTIP, _STATUS, _FACTORY_TYPEID) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Param); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_TEXTURES(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::BaseTexture); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_ANY_SINGLE(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Any, false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_POINT(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Point, false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_SPATIAL(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Spatial, false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_PARAM(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Param, false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_FACTORY(_LABEL, _TOOLTIP, _STATUS, _FACTORY_TYPEID) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Param, false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_TEXTURE(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::BaseTexture, false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#else
#define PCGEX_PIN_ANY(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, FPCGDataTypeInfo::AsId()); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_POINTS(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, FPCGDataTypeInfoPoint::AsId()); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_SPATIALS(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, FPCGDataTypeInfoSpatial::AsId()); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_POLYLINES(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, FPCGDataTypeInfoPolyline::AsId()); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_MESH(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, FPCGDataTypeInfoDynamicMesh::AsId()); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_PARAMS(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, FPCGDataTypeInfoParam::AsId()); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS)  }
#define PCGEX_PIN_FILTERS(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, FPCGExDataTypeInfoFilter::AsId()); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_FACTORIES(_LABEL, _TOOLTIP, _STATUS, _FACTORY_TYPEID) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, _FACTORY_TYPEID); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_TEXTURES(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, FPCGDataTypeInfoBaseTexture2D::AsId()); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_ANY_SINGLE(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, FPCGDataTypeInfo::AsId(), false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_POINT(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, FPCGDataTypeInfoPoint::AsId(), false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_SPATIAL(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, FPCGDataTypeInfoSpatial::AsId(), false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_PARAM(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, FPCGDataTypeInfoParam::AsId(), false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_FACTORY(_LABEL, _TOOLTIP, _STATUS, _FACTORY_TYPEID) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, _FACTORY_TYPEID, false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#define PCGEX_PIN_TEXTURE(_LABEL, _TOOLTIP, _STATUS) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, FPCGDataTypeInfoBaseTexture2D::AsId(), false, false); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) }
#endif

#define PCGEX_PIN_OPERATION_OVERRIDES(_LABEL) PCGEX_PIN_PARAMS(_LABEL, "Property overrides to be forwarded & processed by the module. Name must match the property you're targeting 1:1, type mismatch will be broadcasted at your own risk.", Advanced)

#define PCGEX_BOX_TOLERANCE(_NAME, A, B, Tolerance)\
const FVector _NAME##_A = A;\
const FVector _NAME##_B = B;\
FBox _NAME(_NAME##_A.ComponentMin(_NAME##_B) - Tolerance, _NAME##_A.ComponentMax(_NAME##_B) + Tolerance);

#define PCGEX_BOX_TOLERANCE_INLINE(A, B, Tolerance) FBox(A.ComponentMin(B) - Tolerance, A.ComponentMax(B) + Tolerance)

#define PCGEX_SET_BOX_TOLERANCE(_NAME, A, B, Tolerance)\
const FVector _NAME##_A = A;\
const FVector _NAME##_B = B;\
_NAME = FBox(_NAME##_A.ComponentMin(_NAME##_B) - Tolerance, _NAME##_A.ComponentMax(_NAME##_B) + Tolerance);

#endif
