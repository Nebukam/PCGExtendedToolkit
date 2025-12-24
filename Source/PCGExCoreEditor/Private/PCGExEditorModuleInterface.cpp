// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExEditorModuleInterface.h"
#include "Styling/SlateStyle.h"
#include "Editor.h"
#include "PCGExLog.h"

TArray<IPCGExEditorModuleInterface*> IPCGExEditorModuleInterface::RegisteredModules;

void IPCGExEditorModuleInterface::StartupModule()
{
	RegisteredModules.Add(this);
	UE_LOG(LogPCGEx, Log, TEXT("IPCGExEditorModuleInterface::StartupModule >> %s"), *GetModuleName());
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
