// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEditorModuleInterface.h"

class FPCGExFoundationsEditorModule final : public IPCGExEditorModuleInterface
{
	PCGEX_MODULE_BODY
	
public:
	virtual void StartupModule() override;
};
