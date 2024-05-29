// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristicFeedbackGlobal.h"

void UPCGExHeuristicFeedbackGlobal::PrepareForData(PCGExCluster::FCluster* InCluster)
{
	Super::PrepareForData(InCluster);
}

UPCGExHeuristicOperation* UPCGHeuristicsFactoryFeedbackGlobal::CreateOperation() const
{
	UPCGExHeuristicFeedbackGlobal* NewOperation = NewObject<UPCGExHeuristicFeedbackGlobal>();
	NewOperation->NodeScale = Descriptor.VisitedPointsWeightFactor;
	NewOperation->EdgeScale = Descriptor.VisitedEdgesWeightFactor;
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExHeuristicFeedbackGlobalProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGHeuristicsFactoryFeedbackGlobal* NewHeuristics = NewObject<UPCGHeuristicsFactoryFeedbackGlobal>();
	NewHeuristics->WeightFactor = Descriptor.WeightFactor;
	return Super::CreateFactory(InContext, NewHeuristics);
}
