// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExOperation.h"
#include "PCGParamData.h"

void FPCGExOperation::BindContext(FPCGExContext* InContext)
{
	Context = InContext;
}

#if WITH_EDITOR
void FPCGExOperation::UpdateUserFacingInfos()
{
}
#endif

void FPCGExOperation::RegisterConsumableAttributesWithFacade(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InFacade) const
{
}

void FPCGExOperation::RegisterPrimaryBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) const
{
}

void FPCGExOperation::RegisterAssetDependencies(FPCGExContext* InContext)
{
	
}
