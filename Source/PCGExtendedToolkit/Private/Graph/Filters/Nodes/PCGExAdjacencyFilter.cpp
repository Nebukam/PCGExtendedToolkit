// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/Nodes/PCGExAdjacencyFilter.h"

#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExNodeAdjacencyFilter"
#define PCGEX_NAMESPACE NodeAdjacencyFilter

PCGExPointFilter::TFilter* UPCGExAdjacencyFilterFactory::CreateFilter() const
{
	return new PCGExNodeAdjacency::FAdjacencyFilter(this);
}

namespace PCGExNodeAdjacency
{
	bool FAdjacencyFilter::Init(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster, PCGExData::FFacade* InPointDataFacade, PCGExData::FFacade* InEdgeDataFacade)
	{
		if (!TFilter::Init(InContext, InCluster, InPointDataFacade, InEdgeDataFacade)) { return false; }

		bCaptureFromNodes = TypedFilterFactory->Config.OperandBSource != EPCGExGraphValueSource::Edge;

		if (TypedFilterFactory->Config.CompareAgainst == EPCGExFetchType::Attribute)
		{
			OperandA = PointDataFacade->GetBroadcaster<double>(TypedFilterFactory->Config.OperandA);
			if (!OperandA)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: \"{0}\"."), FText::FromName(TypedFilterFactory->Config.OperandA.GetName())));
				return false;
			}
		}

		if (!Adjacency.Init(InContext, PointDataFacade)) { return false; }

		if (bCaptureFromNodes)
		{
			OperandB = PointDataFacade->GetBroadcaster<double>(TypedFilterFactory->Config.OperandB);
			if (!OperandB)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: \"{0}\"."), FText::FromName(TypedFilterFactory->Config.OperandB.GetName())));
				return false;
			}
		}
		else
		{
			OperandB = EdgeDataFacade->GetBroadcaster<double>(TypedFilterFactory->Config.OperandB);
			if (!OperandB)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: \"{0}\"."), FText::FromName(TypedFilterFactory->Config.OperandB.GetName())));
				return false;
			}
		}

		return true;
	}

	bool FAdjacencyFilter::Test(const PCGExCluster::FNode& Node) const
	{
		const TArray<PCGExCluster::FNode>& NodesRef = *Cluster->Nodes;
		//const TArray<PCGExGraph::FIndexedEdge>& EdgesRef = *CapturedCluster->Edges;

		const double A = OperandA->Values[Node.PointIndex];
		double B = 0;

		if (Adjacency.bTestAllNeighbors)
		{
			// Each adjacent sample must pass the comparison, exit early.
			if (Adjacency.Consolidation == EPCGExAdjacencyGatherMode::Individual)
			{
				if (bCaptureFromNodes)
				{
					for (const uint64 AdjacencyHash : Node.Adjacency)
					{
						B = OperandA->Values[NodesRef[PCGEx::H64A(AdjacencyHash)].PointIndex];
						if (!PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance)) { return false; }
					}
				}
				else
				{
					for (const uint64 AdjacencyHash : Node.Adjacency)
					{
						B = OperandA->Values[PCGEx::H64B(AdjacencyHash)];
						if (!PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance)) { return false; }
					}
				}

				return true;
			}

			// First, build the consolidated operand B
			switch (Adjacency.Consolidation)
			{
			case EPCGExAdjacencyGatherMode::Average:
				if (bCaptureFromNodes)
				{
					for (const uint64 AdjacencyHash : Node.Adjacency) { B += OperandB->Values[NodesRef[PCGEx::H64A(AdjacencyHash)].PointIndex]; }
				}
				else
				{
					for (const uint64 AdjacencyHash : Node.Adjacency) { B += OperandB->Values[PCGEx::H64B(AdjacencyHash)]; }
				}
				B /= Node.Adjacency.Num();
				break;
			case EPCGExAdjacencyGatherMode::Min:
				B = TNumericLimits<double>::Max();
				if (bCaptureFromNodes)
				{
					for (const uint64 AdjacencyHash : Node.Adjacency) { B = FMath::Min(B, OperandB->Values[NodesRef[PCGEx::H64A(AdjacencyHash)].PointIndex]); }
				}
				else
				{
					for (const uint64 AdjacencyHash : Node.Adjacency) { B = FMath::Min(B, OperandB->Values[PCGEx::H64B(AdjacencyHash)]); }
				}
				break;
			case EPCGExAdjacencyGatherMode::Max:
				B = TNumericLimits<double>::Min();
				if (bCaptureFromNodes)
				{
					for (const uint64 AdjacencyHash : Node.Adjacency) { B = FMath::Max(B, OperandB->Values[NodesRef[PCGEx::H64A(AdjacencyHash)].PointIndex]); }
				}
				else
				{
					for (const uint64 AdjacencyHash : Node.Adjacency) { B = FMath::Max(B, OperandB->Values[PCGEx::H64B(AdjacencyHash)]); }
				}
				break;
			case EPCGExAdjacencyGatherMode::Sum:
				if (bCaptureFromNodes)
				{
					for (const uint64 AdjacencyHash : Node.Adjacency) { B += FMath::Max(B, OperandB->Values[NodesRef[PCGEx::H64A(AdjacencyHash)].PointIndex]); }
				}
				else
				{
					for (const uint64 AdjacencyHash : Node.Adjacency) { B += FMath::Max(B, OperandB->Values[PCGEx::H64B(AdjacencyHash)]); }
				}
				break;
			default:
			case EPCGExAdjacencyGatherMode::Individual:
				break;
			}

			return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance);
		}

		// Only some adjacent samples must pass the comparison
		const int32 Threshold = Adjacency.GetThreshold(Node);

		// Early exit on impossible thresholds (i.e node has less neighbor that the minimum or exact requirements)		
		if (Threshold == -1) { return false; }

		// TODO : We can be slightly more efficient and exiting early based on selected threshold mode

		// Test all neighbors individually and check how many succeeded
		int32 LocalSuccessCount = 0;
		if (bCaptureFromNodes)
		{
			for (const uint64 AdjacencyHash : Node.Adjacency)
			{
				B = OperandA->Values[NodesRef[PCGEx::H64A(AdjacencyHash)].PointIndex];
				if (PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance)) { LocalSuccessCount++; }
			}
		}
		else
		{
			for (const uint64 AdjacencyHash : Node.Adjacency)
			{
				B = OperandA->Values[PCGEx::H64B(AdjacencyHash)];
				if (PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance)) { LocalSuccessCount++; }
			}
		}

		return PCGExCompare::Compare(Adjacency.ThresholdComparison, LocalSuccessCount, Threshold, Adjacency.ThresholdTolerance);
	}
}

PCGEX_CREATE_FILTER_FACTORY(Adjacency)

#if WITH_EDITOR
FString UPCGExAdjacencyFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = Config.OperandA.GetName().ToString() + PCGExCompare::ToString(Config.Comparison);

	DisplayName += Config.OperandB.GetName().ToString();
	DisplayName += TEXT(" (");

	switch (Config.Adjacency.Mode)
	{
	case EPCGExAdjacencyTestMode::All:
		DisplayName += TEXT("All");
		break;
	case EPCGExAdjacencyTestMode::Some:
		DisplayName += TEXT("Some");
		break;
	default: ;
	}

	DisplayName += TEXT(")");
	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
