// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Transform/Tensors/PCGExTensorOperation.h"
#include "Transform/Tensors/PCGExTensorFactoryProvider.h"

void UPCGExTensorOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	//if (const UPCGExTensorOperation* TypedOther = Cast<UPCGExTensorOperation>(Other))	{	}
}