// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicSteepness.h"

void UPCGExHeuristicSteepness::PrepareForCluster(PCGExCluster::FCluster* InCluster)
{
	UpwardVector = UpVector.GetSafeNormal();
	Super::PrepareForCluster(InCluster);
}

double UPCGExHeuristicSteepness::GetGlobalScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return SampleCurve(GetDot(From.Position, Goal.Position)) * ReferenceWeight;
}

double UPCGExHeuristicSteepness::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FIndexedEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return SampleCurve(GetDot(From.Position, To.Position)) * ReferenceWeight;
}

double UPCGExHeuristicSteepness::GetDot(const FVector& From, const FVector& To) const
{
	return bInvert ?
		       1-FMath::Abs(FVector::DotProduct((From - To).GetSafeNormal(), UpwardVector)) :
		       FMath::Abs(FVector::DotProduct((From - To).GetSafeNormal(), UpwardVector));
}

UPCGExHeuristicOperation* UPCGHeuristicsFactorySteepness::CreateOperation() const
{
	UPCGExHeuristicSteepness* NewOperation = NewObject<UPCGExHeuristicSteepness>();
	PCGEX_FORWARD_HEURISTIC_DESCRIPTOR
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExHeuristicsSteepnessProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGHeuristicsFactorySteepness* NewHeuristics = NewObject<UPCGHeuristicsFactorySteepness>();
	NewHeuristics->WeightFactor = Descriptor.WeightFactor;
	return Super::CreateFactory(InContext, NewHeuristics);
}

#if WITH_EDITOR
FString UPCGExHeuristicsSteepnessProviderSettings::GetDisplayName() const
{
	return GetDefaultNodeName().ToString()
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Descriptor.WeightFactor) / 1000.0));
}
#endif
