// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristicFeedback.h"

void UPCGExHeuristicFeedback::PrepareForCluster(PCGExCluster::FCluster* InCluster)
{
	Super::PrepareForCluster(InCluster);
}

double UPCGExHeuristicFeedback::GetGlobalScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return NodeExtraWeight[From.NodeIndex];
}

double UPCGExHeuristicFeedback::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FIndexedEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	const double* NodePtr = NodeExtraWeight.Find(To.NodeIndex);
	const double* EdgePtr = EdgeExtraWeight.Find(Edge.EdgeIndex);

	return (NodePtr ? *NodePtr : 0) + (EdgePtr ? *EdgePtr : 0);
}

void UPCGExHeuristicFeedback::FeedbackPointScore(const PCGExCluster::FNode& Node)
{
	double& NodeWeight = NodeExtraWeight.FindOrAdd(Node.PointIndex);
	NodeWeight += ReferenceWeight * NodeScale;
}

void UPCGExHeuristicFeedback::FeedbackScore(const PCGExCluster::FNode& Node, const PCGExGraph::FIndexedEdge& Edge)
{
	double& NodeWeight = NodeExtraWeight.FindOrAdd(Node.PointIndex);
	double& EdgeWeight = NodeExtraWeight.FindOrAdd(Edge.EdgeIndex);
	NodeWeight += ReferenceWeight * NodeScale;
	EdgeWeight += ReferenceWeight * EdgeScale;
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
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExHeuristicFeedbackProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGHeuristicsFactoryFeedback* NewHeuristics = NewObject<UPCGHeuristicsFactoryFeedback>();
	NewHeuristics->WeightFactor = Descriptor.WeightFactor;
	NewHeuristics->Descriptor = Descriptor;
	return Super::CreateFactory(InContext, NewHeuristics);
}
