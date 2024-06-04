// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"

void UPCGExHeuristicOperation::PrepareForCluster(PCGExCluster::FCluster* InCluster)
{
	Cluster = InCluster;
	PCGEX_LOAD_SOFTOBJECT(UCurveFloat, ScoreCurve, ScoreCurveObj, PCGEx::WeightDistributionLinear)

	LocalWeightMultiplier.Empty();

	bHasCustomLocalWeightMultiplier = false;
	if (bUseLocalWeightMultiplier)
	{
		const PCGExData::FPointIO* PointIO = LocalWeightMultiplierSource == EPCGExGraphValueSource::Point ? InCluster->PointsIO : InCluster->EdgesIO;
		const int32 NumPoints = PointIO->GetNum();
		PCGEx::FLocalSingleFieldGetter* Getter = new PCGEx::FLocalSingleFieldGetter();

		Getter->Capture(WeightMultiplierAttribute);

		if (Getter->Grab(*PointIO))
		{
			LocalWeightMultiplier.SetNumZeroed(NumPoints);
			for (int i = 0; i < NumPoints; i++) { LocalWeightMultiplier[i] = Getter->Values[i]; }
			bHasCustomLocalWeightMultiplier = true;
		}

		PCGEX_DELETE(Getter)
	}
}

double UPCGExHeuristicOperation::GetGlobalScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return 0;
}

double UPCGExHeuristicOperation::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FIndexedEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return 0;
}

double UPCGExHeuristicOperation::GetCustomWeightMultiplier(const int32 PointIndex, const int32 EdgeIndex) const
{
	if (!bUseLocalWeightMultiplier || LocalWeightMultiplier.IsEmpty()) { return 1; }
	return FMath::Abs(LocalWeightMultiplier[LocalWeightMultiplierSource == EPCGExGraphValueSource::Point ? PointIndex : EdgeIndex]);
}

void UPCGExHeuristicOperation::Cleanup()
{
	Cluster = nullptr;
	LocalWeightMultiplier.Empty();
	Super::Cleanup();
}

double UPCGExHeuristicOperation::SampleCurve(const double InTime) const
{
	return FMath::Max(0, ScoreCurveObj->GetFloatValue(bInvert ? 1 - InTime : InTime));
}
