// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExDetailsFiltering.generated.h"

UENUM()
enum class EPCGExFilterDataAction : uint8
{
	Keep = 0 UMETA(DisplayName = "Keep", ToolTip="Keeps only selected data"),
	Omit = 1 UMETA(DisplayName = "Omit", ToolTip="Omit selected data from output"),
	Tag  = 2 UMETA(DisplayName = "Tag", ToolTip="Keep all and Tag"),
};
