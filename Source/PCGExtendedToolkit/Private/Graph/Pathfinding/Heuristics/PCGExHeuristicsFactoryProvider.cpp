// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristicsFactoryProvider.h"
#include "Graph/Pathfinding/PCGExPathfinding.h"

#define LOCTEXT_NAMESPACE "PCGExCreateHeuristics"
#define PCGEX_NAMESPACE CreateHeuristics

PCGExFactories::EType UPCGHeuristicsFactoryBase::GetFactoryType() const
{
	return PCGExFactories::EType::Heuristics;
}

UPCGExHeuristicOperation* UPCGHeuristicsFactoryBase::CreateOperation() const
{
	return nullptr; // Create heuristic operation
}

FName UPCGExHeuristicsFactoryProviderSettings::GetMainOutputLabel() const { return PCGExPathfinding::OutputHeuristicsLabel; }

UPCGExParamFactoryBase* UPCGExHeuristicsFactoryProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	return InFactory;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
