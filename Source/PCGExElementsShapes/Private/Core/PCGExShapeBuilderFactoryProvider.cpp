// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExShapeBuilderFactoryProvider.h"
#include "Core/PCGExShapeBuilderOperation.h"

#define LOCTEXT_NAMESPACE "PCGExCreateShapeBuilder"
#define PCGEX_NAMESPACE CreateShapeBuilder

PCG_DEFINE_TYPE_INFO(FPCGExDataTypeInfoShape, UPCGExShapeBuilderFactoryData)

TSharedPtr<FPCGExShapeBuilderOperation> UPCGExShapeBuilderFactoryData::CreateOperation(FPCGExContext* InContext) const
{
	return nullptr; // Create shape builder operation
}

UPCGExFactoryData* UPCGExShapeBuilderFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	return Super::CreateFactory(InContext, InFactory);
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
