// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Transform/Tensors/PCGExTensorOperation.h"
#include "Transform/Tensors/PCGExTensorFactoryProvider.h"

bool UPCGExTensorOperation::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	Factory = InFactory;
	return true;
}

PCGExTensor::FTensorSample UPCGExTensorOperation::Sample(const int32 InSeedIndex, const FTransform& InProbe) const
{
	return PCGExTensor::FTensorSample{};
}

bool UPCGExTensorOperation::PrepareForData(const TSharedPtr<PCGExData::FFacade>& InDataFacade)
{
	PrimaryDataFacade = InDataFacade;
	return true;
}

bool UPCGExTensorPointOperation::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!UPCGExTensorOperation::Init(InContext, InFactory)) { return false; }
	Octree = &InFactory->GetOctree();
	return true;
}


