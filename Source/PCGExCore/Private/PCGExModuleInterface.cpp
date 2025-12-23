// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExModuleInterface.h"

#include "PCGExLog.h"
#include "UObject/CoreRedirects.h"

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
void IPCGExModuleInterface::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle, FPCGDataTypeRegistry& InRegistry)
{
#if PCGEX_SUBMODULE_CORE_REDIRECT_ENABLED

	// Since we moved nodes from the old PCGExtendedToolkit module to their own submodules
	// we need to register redirects.
	// Thankfully, those can be disabled once migration is completed.

	TArray<FCoreRedirect> Redirects;

	const FString ThisModuleName = GetModuleName();
	const FString OldModuleName = TEXT("PCGExtendedToolkit");

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;

		// Check if this class belongs to THIS module
		FString ClassPath = Class->GetPathName();
		if (!ClassPath.StartsWith(FString::Printf(TEXT("/Script/%s."), *ThisModuleName)))
		{
			continue;
		}

		FString ClassName = Class->GetName();

		//UE_LOG(LogPCGEx, Warning, TEXT("Dynamic Redirect : \"/Script/%s.%s\" -> \"/Script/%s.%s\""), *OldModuleName, *ClassName, *ThisModuleName, *ClassName);

		Redirects.Emplace(
			ECoreRedirectFlags::Type_Class,
			*FString::Printf(TEXT("/Script/%s.%s"), *OldModuleName, *ClassName),
			*FString::Printf(TEXT("/Script/%s.%s"), *ThisModuleName, *ClassName));
	}

	if (Redirects.Num() > 0)
	{
		FCoreRedirects::AddRedirectList(Redirects, *ThisModuleName);
		UE_LOG(LogPCGEx, Log, TEXT("%s: Registered %d class redirects"), *ThisModuleName, Redirects.Num());
	}

#endif
}

void IPCGExModuleInterface::RegisterMenuExtensions()
{
}

void IPCGExModuleInterface::UnregisterMenuExtensions()
{
	UToolMenus::UnregisterOwner(this);
}
#endif
