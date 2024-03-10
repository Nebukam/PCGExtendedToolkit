// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicSteepness.h"


void UPCGExHeuristicSteepness::PrepareForData(PCGExCluster::FCluster* InCluster)
{
	UpwardVector = UpVector.GetSafeNormal();

	if (ScoreCurve.IsNull()) { ScoreCurveObj = TSoftObjectPtr<UCurveFloat>(PCGEx::WeightDistributionLinear).LoadSynchronous(); }
	else { ScoreCurveObj = ScoreCurve.LoadSynchronous(); }

	Super::PrepareForData(InCluster);
}

double UPCGExHeuristicSteepness::GetGlobalScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	const double Dot = FMath::Abs(FVector::DotProduct((From.Position - Goal.Position).GetSafeNormal(), UpwardVector));
	return FMath::Max(0, ScoreCurveObj->GetFloatValue((bInvert ? 1 - Dot : Dot))) * ReferenceWeight
	       + bAddDistanceHeuristic ? ((Super::GetGlobalScore(From, Seed, Goal) / ReferenceWeight) * DistanceWeight) : 0;
}

double UPCGExHeuristicSteepness::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FIndexedEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	const double Dot = FMath::Abs(FVector::DotProduct((From.Position - To.Position).GetSafeNormal(), UpwardVector));
	return FMath::Max(0, ScoreCurveObj->GetFloatValue((bInvert ? 1 - Dot : Dot))) * ReferenceWeight
	       + bAddDistanceHeuristic ? ((Super::GetEdgeScore(From, To, Edge, Seed, Goal) / ReferenceWeight) * DistanceWeight) : 0;
}

void UPCGExHeuristicSteepness::ApplyOverrides()
{
	Super::ApplyOverrides();
	PCGEX_OVERRIDE_OP_PROPERTY(bInvert, FName(TEXT("Heuristics/Invert")), EPCGMetadataTypes::Boolean);
	PCGEX_OVERRIDE_OP_PROPERTY(UpVector, FName(TEXT("Heuristics/UpVector")), EPCGMetadataTypes::Vector);
	PCGEX_OVERRIDE_OP_PROPERTY(bAddDistanceHeuristic, FName(TEXT("Heuristics/AddDistanceHeuristic")), EPCGMetadataTypes::Boolean);
	PCGEX_OVERRIDE_OP_PROPERTY(DistanceWeight, FName(TEXT("Heuristics/DistanceWeight")), EPCGMetadataTypes::Double);
}
