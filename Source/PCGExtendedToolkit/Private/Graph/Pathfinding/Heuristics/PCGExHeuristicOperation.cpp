// Copyright Timothé Lapetite 2024
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
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("Invalid Heuristic attribute: \"{0}\"."), FText::FromName(WeightMultiplierAttribute.GetName())));
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
