// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSettingsCacheBody.h"

#define PCGEX_COLLECTIONS_SETTINGS PCGEX_SETTINGS_INST(Collections)

struct PCGEXCOLLECTIONS_API FPCGExCollectionsSettingsCache
{
	PCGEX_SETTING_CACHE_BODY(Collections)
	bool bDisableCollisionByDefault = true;
};
