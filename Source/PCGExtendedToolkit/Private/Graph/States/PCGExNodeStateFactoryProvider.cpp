// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/States/PCGExNodeStateFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExCreateNodeState"
#define PCGEX_NAMESPACE CreateNodeState

PCGExFactories::EType UPCGExNodeStateFactoryBase::GetFactoryType() const
{
	return PCGExFactories::EType::StateNode;
}

UPCGExNodeStateOperation* UPCGExNodeStateFactoryBase::CreateOperation() const
{
	return nullptr; // Create NodeState operation
}

FName UPCGExNodeStateFactoryProviderSettings::GetMainOutputLabel() const { return PCGExGraph::OutputNodeStateLabel; }

UPCGExParamFactoryBase* UPCGExNodeStateFactoryProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	return InFactory;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
