// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensorMagnet.h"

#define LOCTEXT_NAMESPACE "PCGExCreateTensorMagnet"
#define PCGEX_NAMESPACE CreateTensorMagnet

bool UPCGExTensorMagnet::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!Super::Init(InContext, InFactory)) { return false; }
	
	return true;
}

PCGEX_TENSOR_BOILERPLATE(Magnet)

bool UPCGExTensorMagnetFactory::InitInternalData(FPCGExContext* InContext)
{
	if (!Super::InitInternalData(InContext)) { return false; }

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
