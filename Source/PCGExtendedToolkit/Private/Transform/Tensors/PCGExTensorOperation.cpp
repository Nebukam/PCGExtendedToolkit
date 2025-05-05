// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Transform/Tensors/PCGExTensorOperation.h"
#include "Transform/Tensors/PCGExTensorFactoryProvider.h"

bool PCGExTensorOperation::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	Factory = InFactory;
	return true;
}

PCGExTensor::FTensorSample PCGExTensorOperation::Sample(const int32 InSeedIndex, const FTransform& InProbe) const
{
	return PCGExTensor::FTensorSample{};
}

bool PCGExTensorOperation::PrepareForData(const TSharedPtr<PCGExData::FFacade>& InDataFacade)
{
	PrimaryDataFacade = InDataFacade;
	return true;
}

bool PCGExTensorPointOperation::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!PCGExTensorOperation::Init(InContext, InFactory)) { return false; }
	Octree = &InFactory->GetOctree();
	return true;
}


