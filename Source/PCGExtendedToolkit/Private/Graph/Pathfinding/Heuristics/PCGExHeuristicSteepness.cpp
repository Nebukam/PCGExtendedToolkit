// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicSteepness.h"


void UPCGExHeuristicSteepness::PrepareForData(PCGExCluster::FCluster* InCluster)
{
	UpwardVector = UpVector.GetSafeNormal();

	if (!SteepnessScoreCurve || SteepnessScoreCurve.IsNull()) { SteepnessScoreCurveObj = TSoftObjectPtr<UCurveFloat>(PCGEx::WeightDistributionLinear).LoadSynchronous(); }
	else { SteepnessScoreCurveObj = SteepnessScoreCurve.LoadSynchronous(); }

	ReverseWeight = 1 / ReferenceWeight;

	Super::PrepareForData(InCluster);
}

double UPCGExHeuristicSteepness::GetGlobalScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	const double Dot = GetDot(From.Position, Goal.Position);
	const double SampledDot = FMath::Max(0, SteepnessScoreCurveObj->GetFloatValue(Dot)) * ReferenceWeight;
	const double Super = Super::GetGlobalScore(From, Seed, Goal);
	return (SampledDot + Super) * 0.5;
}

double UPCGExHeuristicSteepness::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FIndexedEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	const double Dot = GetDot(From.Position, To.Position);
	const double SampledDot = FMath::Max(0, SteepnessScoreCurveObj->GetFloatValue(Dot)) * ReferenceWeight;
	const double Super = Super::GetEdgeScore(From, To, Edge, Seed, Goal);
	return (SampledDot + Super) * 0.5;
}

double UPCGExHeuristicSteepness::GetDot(const FVector& From, const FVector& To) const
{
	return bInvert ?
		       1 - FMath::Abs(FVector::DotProduct((From - To).GetSafeNormal(), UpwardVector)) :
		       FMath::Abs(FVector::DotProduct((From - To).GetSafeNormal(), UpwardVector));
}

void UPCGExHeuristicSteepness::ApplyOverrides()
{
	Super::ApplyOverrides();
	PCGEX_OVERRIDE_OP_PROPERTY(bInvert, FName(TEXT("Heuristics/Invert")), EPCGMetadataTypes::Boolean);
	PCGEX_OVERRIDE_OP_PROPERTY(UpVector, FName(TEXT("Heuristics/UpVector")), EPCGMetadataTypes::Vector);
}
