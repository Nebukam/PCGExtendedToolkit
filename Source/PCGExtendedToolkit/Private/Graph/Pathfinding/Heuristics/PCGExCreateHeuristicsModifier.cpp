// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExCreateHeuristicsModifier.h"

#include "Graph/Pathfinding/PCGExPathfinding.h"

#define LOCTEXT_NAMESPACE "PCGExCreateHeuristicsModifier"
#define PCGEX_NAMESPACE CreateHeuristicsModifier

FName UPCGExCreateHeuristicsModifierSettings::GetMainOutputLabel() const { return PCGExPathfinding::OutputModifiersLabel; }

UPCGExParamFactoryBase* UPCGExCreateHeuristicsModifierSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGHeuristicsModiferFactory* NewModifier = NewObject<UPCGHeuristicsModiferFactory>();
	NewModifier->Descriptor = Descriptor;

	PCGEX_LOAD_SOFTOBJECT(UCurveFloat, NewModifier->Descriptor.ScoreCurve, NewModifier->Descriptor.ScoreCurveObj, PCGEx::WeightDistributionLinear)
	
	return NewModifier;
}

#if WITH_EDITOR
FString UPCGExCreateHeuristicsModifierSettings::GetDisplayName() const
{
	return Descriptor.Attribute.GetName().ToString()
	+ TEXT(" @ ")
	+  FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Descriptor.WeightFactor) / 1000.0));
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
