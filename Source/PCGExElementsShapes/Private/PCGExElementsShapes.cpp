// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsShapes.h"

#if WITH_EDITOR
#include "Core/PCGExShapeBuilderFactoryProvider.h"
#include "Data/Registry/PCGDataTypeRegistry.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExElementsShapesModule"

void FPCGExElementsShapesModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsShapesModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExElementsShapesModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle, FPCGDataTypeRegistry& InRegistry)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle, InRegistry);
	
	PCGEX_REGISTER_DATA_TYPE(Shape, Shape)
}
#endif

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExElementsShapesModule, PCGExElementsShapes)
