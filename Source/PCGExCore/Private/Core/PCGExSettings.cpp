// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExSettings.h"

#include "Styling/SlateStyle.h"
#include "PCGPin.h"
#include "PCGExCoreMacros.h"
#include "PCGExCoreSettingsCache.h"
#include "PCGExSettingsCacheBody.h"

#include "Helpers/PCGSettingsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExSettings"

#if WITH_EDITOR
bool UPCGExSettings::GetPinExtraIcon(const UPCGPin* InPin, FName& OutExtraIcon, FText& OutTooltip) const
{
	return PCGEX_CORE_SETTINGS.GetPinExtraIcon(InPin, OutExtraIcon, OutTooltip, InPin->IsOutputPin());
}
#endif

bool UPCGExSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	if (PCGEX_CORE_SETTINGS.bToneDownOptionalPins && !InPin->Properties.IsRequiredPin() && !InPin->IsOutputPin()) { return InPin->EdgeCount() > 0; }
	return Super::IsPinUsedByNodeExecution(InPin);
}

PCGExData::EIOInit UPCGExSettings::GetMainDataInitializationPolicy() const { return PCGExData::EIOInit::NoInit; }

#if WITH_EDITOR
void UPCGExSettings::EDITOR_OpenNodeDocumentation() const
{
	const FString META_PCGExDocURL = TEXT("PCGExNodeLibraryDoc");
	const FString META_PCGExDocNodeLibraryBaseURL = TEXT("https://pcgex.gitbook.io/pcgex/node-library/");

	const FString URL = META_PCGExDocNodeLibraryBaseURL + GetClass()->GetMetaData(*META_PCGExDocURL);
	FPlatformProcess::LaunchURL(*URL, nullptr, nullptr);
}
#endif

bool UPCGExSettings::ShouldCache() const
{
	if (!IsCacheable()) { return false; }
	PCGEX_GET_OPTION_STATE(CacheData, bDefaultCacheNodeOutput)
}

bool UPCGExSettings::WantsScopedAttributeGet() const
{
	PCGEX_GET_OPTION_STATE(ScopedAttributeGet, bDefaultScopedAttributeGet)
}

bool UPCGExSettings::WantsBulkInitData() const
{
	PCGEX_GET_OPTION_STATE(BulkInitData, bBulkInitData)
}

#undef LOCTEXT_NAMESPACE
