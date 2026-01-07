// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExCollectionsSettings.h"

#include "CoreMinimal.h"
#include "PCGExCollectionsSettingsCache.h"
#include "PCGExCoreSettingsCache.h"

void UPCGExCollectionsSettings::PostLoad()
{
	Super::PostLoad();
	UpdateSettingsCaches();
}

#if WITH_EDITOR
void UPCGExCollectionsSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateSettingsCaches();
}
#endif

void UPCGExCollectionsSettings::UpdateSettingsCaches() const
{
#define PCGEX_PUSH_SETTING(_MODULE, _SETTING) PCGEX_SETTINGS_INST(_MODULE)._SETTING = _SETTING;

	PCGEX_PUSH_SETTING(Collections, bDisableCollisionByDefault)

#undef PCGEX_PUSH_SETTING
}
