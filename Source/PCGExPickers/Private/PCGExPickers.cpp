// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPickers.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "Core/PCGExPickerFactoryProvider.h"
#include "Data/Registry/PCGDataTypeRegistry.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExPickersModule"

void FPCGExPickersModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExPickersModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExPickersModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle, FPCGDataTypeRegistry& InRegistry)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle, InRegistry);
	
	PCGEX_REGISTER_DATA_TYPE(Picker, Picker)
}
#endif

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExPickersModule, PCGExPickers)
