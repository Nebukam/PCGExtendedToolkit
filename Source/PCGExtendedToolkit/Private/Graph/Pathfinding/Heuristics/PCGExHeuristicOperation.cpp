// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"

void UPCGExHeuristicOperation::PrepareForCluster(const PCGExCluster::FCluster* InCluster)
{
	Cluster = InCluster;
	LocalWeightMultiplier.Empty();

	bHasCustomLocalWeightMultiplier = false;
	if (bUseLocalWeightMultiplier)
	{
		const PCGExData::FPointIO* PointIO = LocalWeightMultiplierSource == EPCGExGraphValueSource::Vtx ? InCluster->VtxIO : InCluster->EdgesIO;
		PCGExData::FFacade* DataFacade = LocalWeightMultiplierSource == EPCGExGraphValueSource::Vtx ? PrimaryDataFacade : SecondaryDataFacade;

		const int32 NumPoints = PointIO->GetNum();
		PCGExData::TCache<double>* LocalWeightCache = DataFacade->GetBroadcaster<double>(WeightMultiplierAttribute);

		if (!LocalWeightCache)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("Invalid Heuristic attribute: \"{0}\"."), FText::FromName(WeightMultiplierAttribute.GetName())));
			return;
		}

		if (LocalWeightMultiplierSource == EPCGExGraphValueSource::Vtx)
		{
			LocalWeightMultiplier.SetNumZeroed(InCluster->Nodes->Num());
			for (const PCGExCluster::FNode& Node : (*InCluster->Nodes)) { LocalWeightMultiplier[Node.NodeIndex] = LocalWeightCache->Values[Node.PointIndex]; }
		}
		else
		{
			LocalWeightMultiplier.SetNumZeroed(NumPoints);
			for (int i = 0; i < NumPoints; i++) { LocalWeightMultiplier[i] = LocalWeightCache->Values[i]; }
		}

		bHasCustomLocalWeightMultiplier = true;
	}
}

void UPCGExHeuristicOperation::Cleanup()
{
	Cluster = nullptr;
	LocalWeightMultiplier.Empty();
	PCGEX_CLEAN_SP(ScoreCurveObj)
	Super::Cleanup();
}
