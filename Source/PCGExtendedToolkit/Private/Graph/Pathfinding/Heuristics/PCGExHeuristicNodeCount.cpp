// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicNodeCount.h"

double UPCGExHeuristicNodeCount::GetEdgeScore(const PCGExCluster::FNode& From, const PCGExCluster::FNode& To, const PCGExGraph::FIndexedEdge& Edge, const PCGExCluster::FNode& Seed, const PCGExCluster::FNode& Goal) const
{
	return ReferenceWeight;
}

UPCGExHeuristicOperation* UPCGHeuristicsFactoryLeastNodes::CreateOperation() const
{
	UPCGExHeuristicNodeCount* NewOperation = NewObject<UPCGExHeuristicNodeCount>();
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExHeuristicsLeastNodesProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGHeuristicsFactoryLeastNodes* NewHeuristics = NewObject<UPCGHeuristicsFactoryLeastNodes>();
	NewHeuristics->WeightFactor = Descriptor.WeightFactor;
	return Super::CreateFactory(InContext, NewHeuristics);
}
