// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristicFeedbackLocal.h"

UPCGExHeuristicOperation* UPCGHeuristicsFactoryFeedbackLocal::CreateOperation() const
{
	UPCGExHeuristicFeedbackLocal* NewOperation = NewObject<UPCGExHeuristicFeedbackLocal>();
	NewOperation->NodeScale = Descriptor.VisitedPointsWeightFactor;
	NewOperation->EdgeScale = Descriptor.VisitedEdgesWeightFactor;
	NewOperation->bGlobalFeedbackLocal = Descriptor.bGlobalFeedbackLocal;
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExHeuristicFeedbackLocalProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGHeuristicsFactoryFeedbackLocal* NewHeuristics = NewObject<UPCGHeuristicsFactoryFeedbackLocal>();
	NewHeuristics->WeightFactor = Descriptor.WeightFactor;
	return Super::CreateFactory(InContext, NewHeuristics);
}
