// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExModuleInterface.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_CLASS(LogPCGEx, Log, All)

class FPCGExCoreModule final : public IPCGExModuleInterface
{
	PCGEX_MODULE_BODY

public:
#if WITH_EDITOR
	virtual void RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle, FPCGDataTypeRegistry& InRegistry) override;
#endif
};
