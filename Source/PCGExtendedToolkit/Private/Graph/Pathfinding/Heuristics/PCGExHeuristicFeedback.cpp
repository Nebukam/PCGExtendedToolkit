// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristicFeedback.h"

void UPCGExHeuristicFeedback::PrepareForData(PCGExCluster::FCluster* InCluster)
{
	Super::PrepareForData(InCluster);

	NodeExtraWeight.SetNumZeroed(InCluster->Nodes.Num());
	EdgeExtraWeight.SetNumZeroed(InCluster->Edges.Num());
}

double UPCGExHeuristicFeedback::GetGlobalScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return 0;
}

double UPCGExHeuristicFeedback::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FIndexedEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return NodeExtraWeight[To.NodeIndex] + EdgeExtraWeight[Edge.EdgeIndex];
}

void UPCGExHeuristicFeedback::FeedbackPointScore(const PCGExCluster::FNode& Node)
{
	NodeExtraWeight[Node.PointIndex] += 1; //TODO : Implement
}

void UPCGExHeuristicFeedback::FeedbackScore(const PCGExCluster::FNode& Node, const PCGExGraph::FIndexedEdge& Edge)
{
	NodeExtraWeight[Node.PointIndex] += 1; //TODO : Implement
	EdgeExtraWeight[Edge.EdgeIndex] += 1; //TODO : Implement
}

void UPCGExHeuristicFeedback::Cleanup()
{
	NodeExtraWeight.Empty();
	EdgeExtraWeight.Empty();
	
	Super::Cleanup();
}

UPCGExHeuristicOperation* UPCGHeuristicsFactoryFeedback::CreateOperation() const
{
	UPCGExHeuristicFeedback* NewOperation = NewObject<UPCGExHeuristicFeedback>();
	NewOperation->NodeScale = Descriptor.VisitedPointsWeightFactor;
	NewOperation->EdgeScale = Descriptor.VisitedEdgesWeightFactor;
	NewOperation->bGlobalFeedback = Descriptor.bGlobalFeedback;
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExHeuristicFeedbackProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGHeuristicsFactoryFeedback* NewHeuristics = NewObject<UPCGHeuristicsFactoryFeedback>();
	NewHeuristics->WeightFactor = Descriptor.WeightFactor;
	return Super::CreateFactory(InContext, NewHeuristics);
}
