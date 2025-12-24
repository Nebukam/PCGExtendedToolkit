// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExModuleInterface.h"

class FPCGExFiltersModule final : public IPCGExModuleInterface
{
	PCGEX_MODULE_BODY

public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

#if WITH_EDITOR
	virtual void RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle) override;
#endif
};
