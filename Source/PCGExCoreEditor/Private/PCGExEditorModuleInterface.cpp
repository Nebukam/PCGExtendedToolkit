// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExEditorModuleInterface.h"

#include "Editor.h"
#include "ContentBrowserMenuContexts.h"

TArray<IPCGExEditorModuleInterface*> IPCGExEditorModuleInterface::RegisteredModules;

void IPCGExEditorModuleInterface::StartupModule()
{
	RegisteredModules.Add(this);
	
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &IPCGExEditorModuleInterface::RegisterMenuExtensions));
}

void IPCGExEditorModuleInterface::ShutdownModule()
{
	RegisteredModules.Remove(this);
	UnregisterMenuExtensions();
}

void IPCGExEditorModuleInterface::RegisterMenuExtensions()
{
}

void IPCGExEditorModuleInterface::UnregisterMenuExtensions()
{
	UToolMenus::UnregisterOwner(this);
}
