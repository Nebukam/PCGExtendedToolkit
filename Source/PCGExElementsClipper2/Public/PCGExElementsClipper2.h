// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#pragma once

#include "CoreMinimal.h"
#include "PCGExModuleInterface.h"
#include "Modules/ModuleManager.h"

class FPCGExElementsClipper2Module : public IPCGExModuleInterface
{
	PCGEX_MODULE_BODY

public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
