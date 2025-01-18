// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristicsFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExCreateHeuristics"
#define PCGEX_NAMESPACE CreateHeuristics

void FPCGExHeuristicConfigBase::Init()
{
	if (!bUseLocalCurve) { LocalScoreCurve.ExternalCurve = ScoreCurve.Get(); }
	ScoreCurveObj = LocalScoreCurve.GetRichCurveConst();
}

UPCGExHeuristicOperation* UPCGExHeuristicsFactoryData::CreateOperation(FPCGExContext* InContext) const
{
	return nullptr; // Create heuristic operation
}

UPCGExFactoryData* UPCGExHeuristicsFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	return InFactory;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
