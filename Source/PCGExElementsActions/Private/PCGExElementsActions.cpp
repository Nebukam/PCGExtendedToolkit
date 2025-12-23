// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsActions.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "Core/PCGExActionFactoryProvider.h"
#include "Data/Registry/PCGDataTypeRegistry.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExActionsModule"

void FPCGExElementsActionsModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsActionsModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExElementsActionsModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle, FPCGDataTypeRegistry& InRegistry)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle, InRegistry);
	
	PCGEX_REGISTER_DATA_TYPE(Action, Action)
}
#endif

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExElementsActionsModule, PCGExElementsActions)
