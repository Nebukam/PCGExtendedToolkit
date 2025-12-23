// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExMatching.h"

#if WITH_EDITOR
#include "Core/PCGExMatchRuleFactoryProvider.h"
#include "Data/Registry/PCGDataTypeRegistry.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExMatchingModule"

void FPCGExMatchingModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExMatchingModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExMatchingModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle, FPCGDataTypeRegistry& InRegistry)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle, InRegistry);

	PCGEX_REGISTER_DATA_TYPE(MatchRule, MatchRule)
}
#endif

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExMatchingModule, PCGExMatching)
