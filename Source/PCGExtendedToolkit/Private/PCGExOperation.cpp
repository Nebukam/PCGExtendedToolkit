// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "PCGExOperation.h"

void UPCGExOperation::BindContext(FPCGExPointsProcessorContext* InContext)
{
	Context = InContext;
}

#if WITH_EDITOR
void UPCGExOperation::UpdateUserFacingInfos()
{
}
#endif

void UPCGExOperation::Cleanup()
{
	Context = nullptr;
}

void UPCGExOperation::Write()
{
}

void UPCGExOperation::BeginDestroy()
{
	Cleanup();
	UObject::BeginDestroy();
}
