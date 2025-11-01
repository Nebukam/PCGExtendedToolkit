// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGData.h"
#include "Data/PCGExDataTag.h"

#include "PCGExDetailsFiltering.generated.h"

namespace PCGExData
{
	class FPointIO;
}

UENUM()
enum class EPCGExFilterDataAction : uint8
{
	Keep = 0 UMETA(DisplayName = "Keep", ToolTip="Keeps only selected data"),
	Omit = 1 UMETA(DisplayName = "Omit", ToolTip="Omit selected data from output"),
	Tag  = 2 UMETA(DisplayName = "Tag", ToolTip="Keep all and Tag"),
};

UENUM()
enum class EPCGExTagsToDataAction : uint8
{
	Ignore     = 0 UMETA(DisplayName = "Do Nothing", Tooltip="Constant."),
	ToData     = 1 UMETA(DisplayName = "To @Data", Tooltip="Copy tag:value to @Data domain attributes."),
	ToElements = 2 UMETA(DisplayName = "Attribute", Tooltip="Copy tag:value to element domain attributes."),
};

namespace PCGEx
{
	PCGEXTENDEDTOOLKIT_API
	void TagsToData(UPCGData* Data, const TSharedPtr<PCGExData::FTags>& Tags, const EPCGExTagsToDataAction Action);
	
	PCGEXTENDEDTOOLKIT_API
	void TagsToData(const TSharedPtr<PCGExData::FPointIO>& Data, const EPCGExTagsToDataAction Action);
}
