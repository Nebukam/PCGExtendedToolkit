// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Shapes/PCGExShapeBuilderFactoryProvider.h"
#include "Shapes/PCGExShapeBuilderOperation.h"

#define LOCTEXT_NAMESPACE "PCGExCreateShapeBuilder"
#define PCGEX_NAMESPACE CreateShapeBuilder

UPCGExShapeBuilderOperation* UPCGExShapeBuilderFactoryData::CreateOperation(FPCGExContext* InContext) const
{
	return nullptr; // Create shape builder operation
}

UPCGExFactoryData* UPCGExShapeBuilderFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	return InFactory;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
