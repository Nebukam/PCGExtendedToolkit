// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "PCGExOperation.h"

void UPCGExOperation::BindContext(FPCGExPointsProcessorContext* InContext)
{
	Context = InContext;
}

void UPCGExOperation::UpdateUserFacingInfos()
{
}

void UPCGExOperation::Cleanup()
{
	Context = nullptr;
}
