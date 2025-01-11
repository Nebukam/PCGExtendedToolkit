// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Transform/Tensors/PCGExTensorOperation.h"

#include "Paths/PCGExPaths.h"
#include "Transform/Tensors/PCGExTensorFactoryProvider.h"

void UPCGExTensorOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	//if (const UPCGExTensorOperation* TypedOther = Cast<UPCGExTensorOperation>(Other))	{	}
}

bool UPCGExTensorOperation::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	Factory = InFactory;
	return true;
}

PCGExTensor::FTensorSample UPCGExTensorOperation::SampleAtPosition(const FVector& InPosition) const
{
	return PCGExTensor::FTensorSample{};
}

void UPCGExTensorPointOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	//if (const UPCGExTensorPointOperation* TypedOther = Cast<UPCGExTensorPointOperation>(Other))	{	}
}

bool UPCGExTensorPointOperation::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!Super::Init(InContext, InFactory)) { return false; }
	Octree = &InFactory->GetOctree();
	return true;
}
