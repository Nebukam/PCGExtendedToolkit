// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExtendedToolkit.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif
#include "PCGExGlobalSettings.h"

#define LOCTEXT_NAMESPACE "FPCGExtendedToolkitModule"

void FPCGExtendedToolkitModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings(
				"Project", "Plugins", "PCGEx",
				LOCTEXT("PCGExDetailsName", "PCGEx"),
				LOCTEXT("PCGExDetailsDescription", "Configure PCG Extended Toolkit settings"),
				GetMutableDefault<UPCGExGlobalSettings>()
			);
	}
#endif
}

void FPCGExtendedToolkitModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module
#if WITH_EDITOR
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "PCGEx");
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExtendedToolkitModule, PCGExtendedToolkit)
