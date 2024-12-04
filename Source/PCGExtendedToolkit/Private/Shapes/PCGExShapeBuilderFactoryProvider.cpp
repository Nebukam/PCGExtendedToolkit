// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Shapes/PCGExShapeBuilderFactoryProvider.h"
#include "Shapes/PCGExShapeBuilderOperation.h"

#define LOCTEXT_NAMESPACE "PCGExCreateShapeBuilder"
#define PCGEX_NAMESPACE CreateShapeBuilder

UPCGExShapeBuilderOperation* UPCGExShapeBuilderFactoryBase::CreateOperation(FPCGExContext* InContext) const
{
	return nullptr; // Create shape builder operation
}

UPCGExParamFactoryBase* UPCGExShapeBuilderFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	return InFactory;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
