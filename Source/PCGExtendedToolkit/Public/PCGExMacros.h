// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#ifndef PCGEX_MACROS
#define PCGEX_MACROS

#pragma region PCGEX MACROS

#define PCGEX_LOG_CTR(_NAME) // if(false){ UE_LOG(LogTemp, Warning, TEXT(#_NAME"::Constructor")) }
#define PCGEX_LOG_DTR(_NAME) // if(false){ UE_LOG(LogTemp, Warning, TEXT(#_NAME"::Destructor")) }

#define FTEXT(_TEXT) FText::FromString(FString(_TEXT))
#define FSTRING(_TEXT) FString(_TEXT)

#define PCGEX_DELETE(_VALUE) if(_VALUE){ delete _VALUE; _VALUE = nullptr; }
#define PCGEX_DELETE_UOBJECT(_VALUE) if(_VALUE){ if (_VALUE->IsRooted()){_VALUE->RemoveFromRoot();} _VALUE->MarkAsGarbage(); _VALUE = nullptr; } // ConditionalBeginDestroy
#define PCGEX_DELETE_OPERATION(_VALUE) if(_VALUE){ _VALUE->Cleanup(); PCGEX_DELETE_UOBJECT(_VALUE) _VALUE = nullptr; } // ConditionalBeginDestroy
#define PCGEX_DELETE_TARRAY(_VALUE) for(const auto* Item : _VALUE){ delete Item; } _VALUE.Empty();
#define PCGEX_DELETE_TARRAY_FULL(_VALUE) if(_VALUE){ for(const auto* Item : (*_VALUE)){ delete Item; } PCGEX_DELETE(_VALUE); }
#define PCGEX_DELETE_TMAP(_VALUE, _TYPE){TArray<_TYPE> Keys; _VALUE.GetKeys(Keys); for (const _TYPE Key : Keys) { delete *_VALUE.Find(Key); } _VALUE.Empty(); Keys.Empty(); }
#define PCGEX_DELETE_FACADE_AND_SOURCE(_VALUE) if(_VALUE){ PCGEX_DELETE(_VALUE->Source) PCGEX_DELETE(_VALUE) }

#define PCGEX_FOREACH_XYZ(MACRO)\
MACRO(X)\
MACRO(Y)\
MACRO(Z)

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 3
enum class EPCGPinStatus : uint8
{
	Normal = 0,
	Required,
	Advanced
};
#endif

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

#define PCGEX_LOAD_SOFTOBJECT(_TYPE, _SOURCE, _TARGET, _DEFAULT)\
if (!_SOURCE.ToSoftObjectPath().IsValid()) { _TARGET = TSoftObjectPtr<_TYPE>(_DEFAULT).LoadSynchronous(); }\
else { _TARGET = _SOURCE.LoadSynchronous(); }\
if (!_TARGET) { _TARGET = TSoftObjectPtr<_TYPE>(_DEFAULT).LoadSynchronous(); }

#pragma endregion

#define PCGEX_SET_NUM(_ARRAY, _NUM) { const int32 _num_ = _NUM; _ARRAY.Reserve(_num_); _ARRAY.SetNum(_num_); }
#define PCGEX_SET_NUM_PTR(_ARRAY, _NUM) { const int32 _num_ = _NUM; _ARRAY->Reserve(_num_); _ARRAY->SetNum(_num_); }

#define PCGEX_SET_NUM_UNINITIALIZED(_ARRAY, _NUM) { const int32 _num_ = _NUM; _ARRAY.Reserve(_num_); _ARRAY.SetNumUninitialized(_num_); }
#define PCGEX_SET_NUM_UNINITIALIZED_NULL(_ARRAY, _NUM) { const int32 _num_ = _NUM; _ARRAY.Reserve(_num_); _ARRAY.SetNumUninitialized(_num_); for(int i = 0; i < _num_; i++){_ARRAY[i] = nullptr;} }
#define PCGEX_SET_NUM_UNINITIALIZED_PTR(_ARRAY, _NUM) { const int32 _num_ = _NUM; _ARRAY->Reserve(_num_); _ARRAY->SetNumUninitialized(_num_); }

#define PCGEX_NODE_INFOS(_SHORTNAME, _NAME, _TOOLTIP)\
virtual FName GetDefaultNodeName() const override { return FName(TEXT(#_SHORTNAME)); } \
virtual FName AdditionalTaskName() const override{ return bCacheResult ? FName(FString("* ")+GetDefaultNodeTitle().ToString()) : FName(GetDefaultNodeTitle().ToString()); }\
virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGEx" #_SHORTNAME, "NodeTitle", "PCGEx | " _NAME);} \
virtual FText GetNodeTooltipText() const override{ return NSLOCTEXT("PCGEx" #_SHORTNAME "Tooltip", "NodeTooltip", _TOOLTIP); }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 3
#define PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(_SHORTNAME, _NAME, _TOOLTIP, _TASK_NAME)\
virtual FName GetDefaultNodeName() const override { return FName(TEXT(#_SHORTNAME)); } \
virtual FName AdditionalTaskName() const override{ return _TASK_NAME.IsNone() ? FName(GetDefaultNodeTitle().ToString()) : FName(FString(GetDefaultNodeTitle().ToString() + "\r" + _TASK_NAME.ToString())); }\
virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGEx" #_SHORTNAME, "NodeTitle", "PCGEx | " _NAME);} \
virtual FText GetNodeTooltipText() const override{ return NSLOCTEXT("PCGEx" #_SHORTNAME "Tooltip", "NodeTooltip", _TOOLTIP); }
#else
#define PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(_SHORTNAME, _NAME, _TOOLTIP, _TASK_NAME)\
virtual FName GetDefaultNodeName() const override { return FName(TEXT(#_SHORTNAME)); } \
virtual FName AdditionalTaskName() const override{ return FName(GetDefaultNodeTitle().ToString()); }\
virtual FString GetAdditionalTitleInformation() const override{ return _TASK_NAME.IsNone() ? FString() : _TASK_NAME.ToString(); }\
virtual bool HasFlippedTitleLines() const { return !_TASK_NAME.IsNone(); }\
virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGEx" #_SHORTNAME, "NodeTitle", "PCGEx | " _NAME);} \
virtual FText GetNodeTooltipText() const override{ return NSLOCTEXT("PCGEx" #_SHORTNAME "Tooltip", "NodeTooltip", _TOOLTIP); }
#endif

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
#define PCGEX_OPERATION_BIND(_NAME, _TYPE) PCGEX_OPERATION_VALIDATE(_NAME) Context->_NAME = Context->RegisterOperation<_TYPE>(Settings->_NAME);
#define PCGEX_VALIDATE_NAME(_NAME) if (!PCGEx::IsValidName(_NAME)){	PCGE_LOG(Error, GraphAndLog, FTEXT("Invalid user-defined attribute name for " #_NAME)); return false;	}
#define PCGEX_VALIDATE_NAME_C(_CTX, _NAME) if (!PCGEx::IsValidName(_NAME)){	PCGE_LOG_C(Error, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); return false;	}
#define PCGEX_SOFT_VALIDATE_NAME(_BOOL, _NAME, _CTX) if(_BOOL){if (!PCGEx::IsValidName(_NAME)){ PCGE_LOG_C(Warning, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); _BOOL = false; } }
#define PCGEX_FWD(_NAME) Context->_NAME = Settings->_NAME;
#define PCGEX_TERMINATE_ASYNC PCGEX_DELETE(AsyncManager)

#define PCGEX_TYPED_CONTEXT_AND_SETTINGS(_NAME) FPCGEx##_NAME##Context* TypedContext = GetContext<FPCGEx##_NAME##Context>(); const UPCGEx##_NAME##Settings* Settings = Context->GetInputSettings<UPCGEx##_NAME##Settings>();	check(Settings);

#define PCGEX_ASYNC_WAIT if (!Context->IsAsyncWorkComplete()) {return false;}

#if WITH_EDITOR
#define PCGEX_PIN_TOOLTIP(_TOOLTIP) Pin.Tooltip = FTEXT(_TOOLTIP);
#else
#define PCGEX_PIN_TOOLTIP(_TOOLTIP) {}
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
#define PCGEX_PIN_STATUS(_STATUS) Pin.PinStatus = EPCGPinStatus::_STATUS;
#else
#define PCGEX_PIN_STATUS(_STATUS) Pin.bAdvancedPin = EPCGPinStatus::_STATUS == EPCGPinStatus::Advanced;
#endif

#define PCGEX_PIN_ANY(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Any); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_POINTS(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Point); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_POLYLINES(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::PolyLine); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_PARAMS(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Param); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_POINT(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Point, false, true); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }
#define PCGEX_PIN_PARAM(_LABEL, _TOOLTIP, _STATUS, _EXTRA) { FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(_LABEL, EPCGDataType::Param, false, true); PCGEX_PIN_TOOLTIP(_TOOLTIP) PCGEX_PIN_STATUS(_STATUS) _EXTRA }

#endif
