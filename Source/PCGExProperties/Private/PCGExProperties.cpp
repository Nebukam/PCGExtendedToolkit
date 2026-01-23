// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExProperties.h"

#if WITH_EDITOR
void FPCGExPropertiesModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle);
	// No special editor registration needed for properties module
}
#endif

PCGEX_IMPLEMENT_MODULE(FPCGExPropertiesModule, PCGExProperties)
