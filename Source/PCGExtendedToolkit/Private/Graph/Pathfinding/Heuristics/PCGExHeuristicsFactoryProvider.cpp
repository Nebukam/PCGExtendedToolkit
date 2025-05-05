// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristicsFactoryProvider.h"




#define LOCTEXT_NAMESPACE "PCGExCreateHeuristics"
#define PCGEX_NAMESPACE CreateHeuristics

void FPCGExHeuristicConfigBase::Init()
{
	if (!bUseLocalCurve)
	{
		PCGExHelpers::LoadBlocking_AnyThread(ScoreCurve);
		LocalScoreCurve.ExternalCurve = ScoreCurve.Get();
	}
	ScoreCurveObj = LocalScoreCurve.GetRichCurveConst();
}

bool UPCGExHeuristicsFactoryData::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_CONDITIONAL(ConfigBase.bUseLocalWeightMultiplier, ConfigBase.WeightMultiplierAttribute, Consumable)

	return true;
}

TSharedPtr<UPCGExHeuristicOperation> UPCGExHeuristicsFactoryData::CreateOperation(FPCGExContext* InContext) const
{
	return nullptr; // Create heuristic operation
}

UPCGExFactoryData* UPCGExHeuristicsFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	return InFactory;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
