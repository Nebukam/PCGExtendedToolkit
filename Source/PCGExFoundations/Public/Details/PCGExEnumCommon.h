// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "CoreMinimal.h"

UENUM()
enum class EPCGExEnumConstantSourceType : uint8
{
	Picker   = 0 UMETA(DisplayName="Picker", Tooltip="Browse through Blueprint enums."),
	Selector = 1 UMETA(DisplayName="Selector", ToolTip="Browse through CPP enums."),
};
