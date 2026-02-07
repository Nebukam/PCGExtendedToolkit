// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Details/PCGExInputShorthandsDetails.h"

#include "Data/PCGExDataHelpers.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Details/PCGExSettingsDetails.h"

#define PCGEX_FOREACH_INPUT_SHORTHAND(MACRO, ...) \
MACRO(bool, Boolean, __VA_ARGS__)       \
MACRO(int32, Integer32, __VA_ARGS__)      \
MACRO(int32, Integer32Abs, __VA_ARGS__)      \
MACRO(int32, Integer3201, __VA_ARGS__)      \
MACRO(float, Float, __VA_ARGS__)      \
MACRO(double, Double, __VA_ARGS__)     \
MACRO(double, DoubleAbs, __VA_ARGS__)     \
MACRO(double, Double01, __VA_ARGS__)     \
MACRO(FVector2D, Vector2, __VA_ARGS__)  \
MACRO(FVector, Vector, __VA_ARGS__)    \
MACRO(FVector, Direction, __VA_ARGS__)    \
MACRO(FVector4, Vector4, __VA_ARGS__)   \
MACRO(FRotator, Rotator, __VA_ARGS__)   \
MACRO(FTransform, Transform, __VA_ARGS__) \
MACRO(FString, String, __VA_ARGS__)    \
MACRO(FName, Name, __VA_ARGS__)

#define PCGEX_SHORTHAND_UPDATE__NAME_IMPL(_TYPE, _NAME)\
void FPCGExInputShorthandName##_NAME::Update(EPCGExInputValueType InInputType, FPCGAttributePropertyInputSelector InSelector, _TYPE InConstant){Input = InInputType; Constant = InConstant;	Attribute = InSelector.GetName();}\
void FPCGExInputShorthandName##_NAME::Update(EPCGExInputValueType InInputType, FName InSelector, _TYPE InConstant){Input = InInputType; Constant = InConstant;	Attribute = InSelector;}\
bool FPCGExInputShorthandName##_NAME::CanSupportDataOnly() const { return Input == EPCGExInputValueType::Constant ? true : PCGExMetaHelpers::IsDataDomainAttribute(Attribute); }

#define PCGEX_SHORTHAND_UPDATE__SELECTOR_IMPL(_TYPE, _NAME)\
void FPCGExInputShorthandSelector##_NAME::Update(EPCGExInputValueType InInputType, FPCGAttributePropertyInputSelector InSelector, _TYPE InConstant){Input = InInputType; Constant = InConstant;	Attribute = InSelector;}\
void FPCGExInputShorthandSelector##_NAME::Update(EPCGExInputValueType InInputType, FName InSelector, _TYPE InConstant){Input = InInputType; Constant = InConstant;	Attribute.Update(InSelector.ToString());}\
bool FPCGExInputShorthandSelector##_NAME::CanSupportDataOnly() const { return Input == EPCGExInputValueType::Constant ? true : PCGExMetaHelpers::IsDataDomainAttribute(Attribute); }

#define PCGEX_TPL_SHORTHAND_NAME(_TYPE, _NAME, ...)\
PCGEX_SETTING_VALUE_IMPL(FPCGExInputShorthandName##_NAME, , _TYPE, Input, Attribute, Constant)\
bool FPCGExInputShorthandName##_NAME::TryReadDataValue(const TSharedPtr<PCGExData::FPointIO>& IO, _TYPE& OutValue, const bool bQuiet) const{return PCGExData::Helpers::TryGetSettingDataValue(IO, Input, Attribute, Constant, OutValue, bQuiet);}\
bool FPCGExInputShorthandName##_NAME::TryReadDataValue(FPCGExContext* InContext, const UPCGData* InData, _TYPE& OutValue, const bool bQuiet) const{return PCGExData::Helpers::TryGetSettingDataValue(InContext, InData, Input, Attribute, Constant, OutValue, bQuiet);}\
void FPCGExInputShorthandName##_NAME::RegisterBufferDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const { if (Input == EPCGExInputValueType::Attribute) { FacadePreloader.Register<_TYPE>(InContext, Attribute); } }\
PCGEX_SHORTHAND_UPDATE__NAME_IMPL(_TYPE, _NAME)

#define PCGEX_TPL_SHORTHAND_SELECTOR(_TYPE, _NAME, ...)\
PCGEX_SETTING_VALUE_IMPL(FPCGExInputShorthandSelector##_NAME, , _TYPE, Input, Attribute, Constant)\
bool FPCGExInputShorthandSelector##_NAME::TryReadDataValue(const TSharedPtr<PCGExData::FPointIO>& IO, _TYPE& OutValue, const bool bQuiet) const{return PCGExData::Helpers::TryGetSettingDataValue(IO, Input, Attribute, Constant, OutValue, bQuiet);}\
bool FPCGExInputShorthandSelector##_NAME::TryReadDataValue(FPCGExContext* InContext, const UPCGData* InData, _TYPE& OutValue, const bool bQuiet) const{return PCGExData::Helpers::TryGetSettingDataValue(InContext, InData, Input, Attribute, Constant, OutValue, bQuiet);}\
void FPCGExInputShorthandSelector##_NAME::RegisterBufferDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const { if (Input == EPCGExInputValueType::Attribute) { FacadePreloader.Register<_TYPE>(InContext, Attribute); } }\
PCGEX_SHORTHAND_UPDATE__SELECTOR_IMPL(_TYPE, _NAME)

PCGEX_FOREACH_INPUT_SHORTHAND(PCGEX_TPL_SHORTHAND_NAME)
PCGEX_FOREACH_INPUT_SHORTHAND(PCGEX_TPL_SHORTHAND_SELECTOR)

#undef PCGEX_TPL_SHORTHAND_NAME
#undef PCGEX_TPL_SHORTHAND_SELECTOR
#undef PCGEX_FOREACH_INPUT_SHORTHAND
