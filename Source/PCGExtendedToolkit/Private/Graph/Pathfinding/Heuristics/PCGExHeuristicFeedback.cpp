// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristicFeedback.h"

void UPCGExHeuristicFeedback::PrepareForCluster(PCGExCluster::FCluster* InCluster)
{
	MaxNodeWeight = 0;
	MaxEdgeWeight = 0;
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

	return ((NodePtr ? SampleCurve(*NodePtr / MaxNodeWeight) * ReferenceWeight : 0) + (EdgePtr ? SampleCurve(*EdgePtr / MaxEdgeWeight) * ReferenceWeight : 0));
}

void UPCGExHeuristicFeedback::FeedbackPointScore(const PCGExCluster::FNode& Node)
{
	double& NodeWeight = NodeExtraWeight.FindOrAdd(Node.PointIndex);
	MaxNodeWeight = FMath::Max(MaxNodeWeight, NodeWeight += ReferenceWeight * NodeScale);
}

void UPCGExHeuristicFeedback::FeedbackScore(const PCGExCluster::FNode& Node, const PCGExGraph::FIndexedEdge& Edge)
{
	double& NodeWeight = NodeExtraWeight.FindOrAdd(Node.PointIndex);
	double& EdgeWeight = NodeExtraWeight.FindOrAdd(Edge.EdgeIndex);
	MaxNodeWeight = FMath::Max(MaxNodeWeight, NodeWeight += ReferenceWeight * NodeScale);
	MaxEdgeWeight = FMath::Max(MaxEdgeWeight, EdgeWeight += ReferenceWeight * EdgeScale);
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
	PCGEX_FORWARD_HEURISTIC_DESCRIPTOR
	NewOperation->NodeScale = Descriptor.VisitedPointsWeightFactor;
	NewOperation->EdgeScale = Descriptor.VisitedEdgesWeightFactor;
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExHeuristicFeedbackProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGHeuristicsFactoryFeedback* NewFactory = NewObject<UPCGHeuristicsFactoryFeedback>();
	PCGEX_FORWARD_HEURISTIC_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExHeuristicFeedbackProviderSettings::GetDisplayName() const
{
	return GetDefaultNodeName().ToString()
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Descriptor.WeightFactor) / 1000.0));
}
#endif
