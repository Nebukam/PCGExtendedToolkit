// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExModuleInterface.h"

#if WITH_EDITOR
#include "Editor.h"
#include "ContentBrowserMenuContexts.h"
#endif

TArray<IPCGExModuleInterface*> IPCGExModuleInterface::RegisteredModules;

void IPCGExModuleInterface::StartupModule()
{
	RegisteredModules.Add(this);
}

void IPCGExModuleInterface::ShutdownModule()
{
	RegisteredModules.Remove(this);

#if WITH_EDITOR
	UnregisterMenuExtensions();
#endif
}

#if WITH_EDITOR
void IPCGExModuleInterface::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle, FPCGDataTypeRegistry& InRegistry)
{
}

void IPCGExModuleInterface::RegisterMenuExtensions()
{
}

void IPCGExModuleInterface::UnregisterMenuExtensions()
{
	UToolMenus::UnregisterOwner(this);
}
#endif
