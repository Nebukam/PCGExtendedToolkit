// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"


void UPCGExHeuristicOperation::PrepareForCluster(const TSharedPtr<const PCGExCluster::FCluster>& InCluster)
{
	Cluster = InCluster;
	LocalWeightMultiplier.Empty();

	bHasCustomLocalWeightMultiplier = false;
	if (bUseLocalWeightMultiplier)
	{
		const TSharedPtr<PCGExData::FPointIO> PointIO = LocalWeightMultiplierSource == EPCGExClusterComponentSource::Vtx ? InCluster->VtxIO.Pin() : InCluster->EdgesIO.Pin();
		const TSharedPtr<PCGExData::FFacade> DataFacade = LocalWeightMultiplierSource == EPCGExClusterComponentSource::Vtx ? PrimaryDataFacade : SecondaryDataFacade;

		const int32 NumPoints = PointIO->GetNum();
		const TSharedPtr<PCGExData::TBuffer<double>> LocalWeightCache = DataFacade->GetBroadcaster<double>(WeightMultiplierAttribute);

		if (!LocalWeightCache)
		{
			PCGEX_LOG_INVALID_SELECTOR_C(Context, "Weight Multiplier (Heuristics)", WeightMultiplierAttribute)
			return;
		}

		if (LocalWeightMultiplierSource == EPCGExClusterComponentSource::Vtx)
		{
			LocalWeightMultiplier.SetNumZeroed(InCluster->Nodes->Num());
			for (const PCGExCluster::FNode& Node : (*InCluster->Nodes)) { LocalWeightMultiplier[Node.Index] = LocalWeightCache->Read(Node.PointIndex); }
		}
		else
		{
			LocalWeightMultiplier.SetNumZeroed(NumPoints);
			for (int i = 0; i < NumPoints; i++) { LocalWeightMultiplier[i] = LocalWeightCache->Read(i); }
		}

		bHasCustomLocalWeightMultiplier = true;
	}
}

double UPCGExHeuristicOperation::GetGlobalScore(const PCGExCluster::FNode& From, const PCGExCluster::FNode& Seed, const PCGExCluster::FNode& Goal) const
{
	return GetScoreInternal(0);
}

double UPCGExHeuristicOperation::GetEdgeScore(const PCGExCluster::FNode& From, const PCGExCluster::FNode& To, const PCGExGraph::FEdge& Edge, const PCGExCluster::FNode& Seed, const PCGExCluster::FNode& Goal, const TSharedPtr<PCGEx::FHashLookup> TravelStack) const
{
	return GetScoreInternal(0);
}

double UPCGExHeuristicOperation::GetCustomWeightMultiplier(const int32 PointIndex, const int32 EdgeIndex) const
{
	//TODO Rewrite this
	if (!bUseLocalWeightMultiplier || LocalWeightMultiplier.IsEmpty()) { return 1; }
	return FMath::Abs(LocalWeightMultiplier[LocalWeightMultiplierSource == EPCGExClusterComponentSource::Vtx ? PointIndex : EdgeIndex]);
}

double UPCGExHeuristicOperation::GetScoreInternal(const double InTime) const
{
	return FMath::Max(0, ScoreCurve->Eval(bInvert ? 1 - InTime : InTime)) * ReferenceWeight;
}
