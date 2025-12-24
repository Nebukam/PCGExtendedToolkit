// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsTensors.h"


#if WITH_EDITOR
#include "Core/PCGExTensorFactoryProvider.h"
#include "Data/Registry/PCGDataTypeRegistry.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExElementsTensorsModule"

void FPCGExElementsTensorsModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsTensorsModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExElementsTensorsModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle);

	PCGEX_START_PCG_REGISTRATION
	PCGEX_REGISTER_DATA_TYPE(Tensor, Tensor)
}
#endif

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExElementsTensorsModule, PCGExElementsTensors)
