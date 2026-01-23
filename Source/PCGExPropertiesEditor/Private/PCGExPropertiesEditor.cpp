// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPropertiesEditor.h"

#include "PropertyEditorModule.h"
#include "Details/PCGExPropertyOverridesCustomization.h"
#include "Details/PCGExPropertyOverrideEntryCustomization.h"
#include "PCGExPropertyCompiled.h"

#define LOCTEXT_NAMESPACE "FPCGExPropertiesEditorModule"

void FPCGExPropertiesEditorModule::StartupModule()
{
	IPCGExEditorModuleInterface::StartupModule();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// Register FPCGExPropertyOverrides customization - provides toggle-checkbox UI
	// Used by Collections (entry overrides) and Tuple (row values)
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FPCGExPropertyOverrides::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGExPropertyOverridesCustomization::MakeInstance)
	);

	// Register FPCGExPropertyOverrideEntry customization - handles individual entry display
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FPCGExPropertyOverrideEntry::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGExPropertyOverrideEntryCustomization::MakeInstance)
	);
}

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExPropertiesEditorModule, PCGExPropertiesEditor)
