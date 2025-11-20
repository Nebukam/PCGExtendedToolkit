// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Details/PCGExDetailsInputShorthands.h"

#include "PCGEx.h"
#include "Details/PCGExMacros.h"
#include "Details/PCGExDetailsSettings.h"

#define PCGEX_FOREACH_INPUT_SHORTHAND(MACRO, ...) \
MACRO(bool, Boolean, __VA_ARGS__)       \
MACRO(int32, Integer32, __VA_ARGS__)      \
MACRO(float, Float, __VA_ARGS__)      \
MACRO(double, Double, __VA_ARGS__)     \
MACRO(FVector2D, Vector2, __VA_ARGS__)  \
MACRO(FVector, Vector, __VA_ARGS__)    \
MACRO(FVector, Direction, __VA_ARGS__)    \
MACRO(FVector4, Vector4, __VA_ARGS__)   \
MACRO(FRotator, Rotator, __VA_ARGS__)   \
MACRO(FTransform, Transform, __VA_ARGS__) \
MACRO(FString, String, __VA_ARGS__)    \
MACRO(FName, Name, __VA_ARGS__)

#define PCGEX_TPL_SHORTHAND_NAME(_TYPE, _NAME, ...) PCGEX_SETTING_VALUE_IMPL(FPCGExInputShorthandName##_NAME, , _TYPE, Input, Attribute, Constant)
#define PCGEX_TPL_SHORTHAND_SELECTOR(_TYPE, _NAME, ...) PCGEX_SETTING_VALUE_IMPL(FPCGExInputShorthandSelector##_NAME, , _TYPE, Input, Attribute, Constant)

PCGEX_FOREACH_INPUT_SHORTHAND(PCGEX_TPL_SHORTHAND_NAME)
PCGEX_FOREACH_INPUT_SHORTHAND(PCGEX_TPL_SHORTHAND_SELECTOR)

#undef PCGEX_TPL_SHORTHAND_NAME
#undef PCGEX_FOREACH_INPUT_SHORTHAND