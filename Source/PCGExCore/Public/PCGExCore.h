// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExLegacyModuleInterface.h"
#include "Modules/ModuleManager.h"



class FPCGExCoreModule final : public IPCGExLegacyModuleInterface
{
	PCGEX_MODULE_BODY

public:
#if WITH_EDITOR
	virtual void RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle) override;
#endif
};
