// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"


UENUM()
enum class EPCGExCellOutputMode : uint8
{
	Paths = 0 UMETA(DisplayName = "Paths", ToolTip="Output cells as paths"),
	AABB  = 1 UMETA(DisplayName = "AABB Bounds", ToolTip="Output cells as AABB bounds"),
	OOB   = 2 UMETA(DisplayName = "OOB Bounds", ToolTip="Output cells as OOB bounds"),
};

namespace PCGExClusters
{
}
