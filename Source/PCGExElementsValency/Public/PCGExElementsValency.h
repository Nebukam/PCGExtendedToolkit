// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExModuleInterface.h"
#include "Modules/ModuleManager.h"

class FPCGExElementsValencyModule : public IPCGExModuleInterface
{
	PCGEX_MODULE_BODY

public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
