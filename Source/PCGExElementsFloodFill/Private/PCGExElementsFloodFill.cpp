// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsFloodFill.h"

#include "Core/PCGExFillControlsFactoryProvider.h"

#if WITH_EDITOR

#if PCGEX_ENGINE_VERSION > 506
#include "Data/Registry/PCGDataTypeRegistry.h"
#endif

#include "Core/PCGExFillControlsFactoryProvider.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExElementsFloodFill"

void FPCGExElementsFloodFill::StartupModule()
{
	OldBaseModules.Add(TEXT("PCGExElementsClusters"));
	IPCGExLegacyModuleInterface::StartupModule();
}

void FPCGExElementsFloodFill::ShutdownModule()
{
	IPCGExLegacyModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExElementsFloodFill::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle);

	PCGEX_START_PCG_REGISTRATION
	PCGEX_REGISTER_DATA_TYPE(FillControl, FillControl)
}
#endif

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExElementsFloodFill, PCGExElementsFloodFill)
