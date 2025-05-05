// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExOperation.h"
#include "PCGParamData.h"

void UPCGExOperation::BindContext(FPCGExContext* InContext)
{
	Context = InContext;
}

#if WITH_EDITOR
void UPCGExOperation::UpdateUserFacingInfos()
{
}
#endif

void UPCGExOperation::RegisterConsumableAttributesWithFacade(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InFacade) const
{
}

void UPCGExOperation::RegisterPrimaryBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) const
{
}

void UPCGExOperation::RegisterAssetDependencies(FPCGExContext* InContext)
{
	
}
