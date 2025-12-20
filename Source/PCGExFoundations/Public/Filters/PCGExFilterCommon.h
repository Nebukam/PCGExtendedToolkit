// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

UENUM()
enum class EPCGExFilterGroupMode : uint8
{
	AND = 0 UMETA(DisplayName = "And", ToolTip="All connected filters must pass.", ActionIcon="PCGEx.Pin.OUT_Filter", SearchHints = "And Combine"),
	OR  = 1 UMETA(DisplayName = "Or", ToolTip="Only a single connected filter must pass.", ActionIcon="PCGEx.Pin.OUT_Filter", SearchHints = "Or Combine")
};