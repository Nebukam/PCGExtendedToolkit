// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExCreateHeuristicsModifier.h"

#include "Graph/Pathfinding/PCGExPathfinding.h"

#define LOCTEXT_NAMESPACE "PCGExCreateHeuristicsModifier"
#define PCGEX_NAMESPACE CreateHeuristicsModifier

FName UPCGExCreateHeuristicsModifierSettings::GetMainOutputLabel() const { return PCGExPathfinding::OutputModifiersLabel; }

UPCGExParamFactoryBase* UPCGExCreateHeuristicsModifierSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	return Super::CreateFactory(InContext, InFactory);
}

FString UPCGExCreateHeuristicsModifierSettings::GetDisplayName() const
{
	// TODO: Grab attribute name
	return Super::GetDisplayName();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
