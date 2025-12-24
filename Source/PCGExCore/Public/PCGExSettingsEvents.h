// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

// Forward declare - no coupling to actual UObject
DECLARE_MULTICAST_DELEGATE(FPCGExSettingsChanged);

struct PCGEXCORE_API FPCGExSettingsEvents
{
	static FPCGExSettingsChanged OnSettingsChanged;
};
