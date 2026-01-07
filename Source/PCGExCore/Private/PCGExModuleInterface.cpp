// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExModuleInterface.h"

#include "PCGExLog.h"
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

TArray<IPCGExModuleInterface*> IPCGExModuleInterface::RegisteredModules;

void IPCGExModuleInterface::StartupModule()
{
	UE_LOG(LogPCGEx, Log, TEXT("IPCGExModuleInterface::StartupModule >> %s"), *GetModuleName());
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
void IPCGExModuleInterface::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle)
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
