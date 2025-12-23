// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

class FPCGDataTypeRegistry;

class PCGEXCOREEDITOR_API IPCGExEditorModuleInterface : public IModuleInterface
{
public:
	// Static registry
	static TArray<IPCGExEditorModuleInterface*> RegisteredModules;

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual void RegisterMenuExtensions();
	virtual void UnregisterMenuExtensions();
};
