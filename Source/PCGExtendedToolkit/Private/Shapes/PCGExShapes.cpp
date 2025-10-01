// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Shapes/PCGExShapes.h"
#include "Details/PCGExDetailsSettings.h"

PCGEX_SETTING_VALUE_GET_IMPL(FPCGExShapeConfigBase, Resolution, double, ResolutionInput, ResolutionAttribute, ResolutionConstant);
PCGEX_SETTING_VALUE_GET_IMPL(FPCGExShapeConfigBase, ResolutionVector, FVector, ResolutionInput, ResolutionAttribute, ResolutionConstantVector);
