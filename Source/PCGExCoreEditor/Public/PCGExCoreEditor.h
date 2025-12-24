// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEditorModuleInterface.h"
#include "Modules/ModuleManager.h"



class FPCGExCoreEditorModule final : public IPCGExEditorModuleInterface
{
	
	PCGEX_MODULE_BODY
	
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
