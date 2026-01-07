// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Details/PCGExInputShorthandsDetails.h"

#include "Data/PCGExDataHelpers.h"
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

#define PCGEX_TPL_SHORTHAND_NAME(_TYPE, _NAME, ...)\
PCGEX_SETTING_VALUE_IMPL(FPCGExInputShorthandName##_NAME, , _TYPE, Input, Attribute, Constant)\
bool FPCGExInputShorthandName##_NAME::TryReadDataValue(const TSharedPtr<PCGExData::FPointIO>& IO, _TYPE& OutValue, const bool bQuiet) const{return PCGExData::Helpers::TryGetSettingDataValue(IO, Input, Attribute, Constant, OutValue, bQuiet);}\
bool FPCGExInputShorthandName##_NAME::TryReadDataValue(FPCGExContext* InContext, const UPCGData* InData, _TYPE& OutValue, const bool bQuiet) const{return PCGExData::Helpers::TryGetSettingDataValue(InContext, InData, Input, Attribute, Constant, OutValue, bQuiet);}

#define PCGEX_TPL_SHORTHAND_SELECTOR(_TYPE, _NAME, ...)\
PCGEX_SETTING_VALUE_IMPL(FPCGExInputShorthandSelector##_NAME, , _TYPE, Input, Attribute, Constant)\
bool FPCGExInputShorthandSelector##_NAME::TryReadDataValue(const TSharedPtr<PCGExData::FPointIO>& IO, _TYPE& OutValue, const bool bQuiet) const{return PCGExData::Helpers::TryGetSettingDataValue(IO, Input, Attribute, Constant, OutValue, bQuiet);}\
bool FPCGExInputShorthandSelector##_NAME::TryReadDataValue(FPCGExContext* InContext, const UPCGData* InData, _TYPE& OutValue, const bool bQuiet) const{return PCGExData::Helpers::TryGetSettingDataValue(InContext, InData, Input, Attribute, Constant, OutValue, bQuiet);}

PCGEX_FOREACH_INPUT_SHORTHAND(PCGEX_TPL_SHORTHAND_NAME)
PCGEX_FOREACH_INPUT_SHORTHAND(PCGEX_TPL_SHORTHAND_SELECTOR)

#undef PCGEX_TPL_SHORTHAND_NAME
#undef PCGEX_FOREACH_INPUT_SHORTHAND
