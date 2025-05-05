// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExOperation.h"
#include "PCGParamData.h"

void PCGExOperation::BindContext(FPCGExContext* InContext)
{
	Context = InContext;
}

#if WITH_EDITOR
void PCGExOperation::UpdateUserFacingInfos()
{
}
#endif

void PCGExOperation::RegisterConsumableAttributesWithFacade(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InFacade) const
{
}

void PCGExOperation::RegisterPrimaryBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) const
{
}

void PCGExOperation::RegisterAssetDependencies(FPCGExContext* InContext)
{
	
}
