// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

namespace PCGExDetailsCustomization
{
	PCGEXTENDEDTOOLKITEDITOR_API void RegisterDetailsCustomization(const TSharedPtr<FSlateStyleSet>& Style);
}
