// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPropertiesEditor.h"

#if WITH_EDITOR
void FPCGExPropertiesEditorModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle);

	// Property customizations can be registered here when needed
	// FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	// PropertyModule.RegisterCustomPropertyTypeLayout(...);
}
#endif

PCGEX_IMPLEMENT_MODULE(FPCGExPropertiesEditorModule, PCGExPropertiesEditor)
