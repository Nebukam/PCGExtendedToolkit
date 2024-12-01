// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/Nodes/PCGExNodeAdjacencyFilter.h"


#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExNodeAdjacencyFilter"
#define PCGEX_NAMESPACE NodeAdjacencyFilter

void UPCGExNodeAdjacencyFilterFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { FacadePreloader.Register<double>(InContext, Config.OperandA); }
	if (Config.OperandBSource == EPCGExClusterComponentSource::Vtx) { FacadePreloader.Register<double>(InContext, Config.OperandB); }
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExNodeAdjacencyFilterFactory::CreateFilter() const
{
	return MakeShared<FNodeAdjacencyFilter>(this);
}

bool FNodeAdjacencyFilter::Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
{
	if (!FFilter::Init(InContext, InCluster, InPointDataFacade, InEdgeDataFacade)) { return false; }

	bCaptureFromNodes = TypedFilterFactory->Config.OperandBSource != EPCGExClusterComponentSource::Edge;

	if (TypedFilterFactory->Config.CompareAgainst == EPCGExInputValueType::Attribute)
	{
		OperandA = PointDataFacade->GetBroadcaster<double>(TypedFilterFactory->Config.OperandA);
		if (!OperandA)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: \"{0}\"."), FText::FromName(TypedFilterFactory->Config.OperandA.GetName())));
			return false;
		}
	}

	if (!Adjacency.Init(InContext, PointDataFacade.ToSharedRef())) { return false; }

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

#define PCGEX_SUB_TEST_FUNC TestSubFunc = [&](const PCGExCluster::FNode& Node, const TArray<PCGExCluster::FNode>& NodesRef, const double A)

	if (Adjacency.bTestAllNeighbors)
	{
		// Each adjacent sample must pass the comparison, exit early.
		if (Adjacency.Consolidation == EPCGExAdjacencyGatherMode::Individual)
		{
			if (bCaptureFromNodes)
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = 0;
					for (const PCGExGraph::FLink Lk : Node.Links)
					{
						B = OperandA->Read(NodesRef[Lk.Node].PointIndex);
						if (!PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance)) { return false; }
					}
					return true;
				};
			}
			else
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = 0;
					for (const PCGExGraph::FLink Lk : Node.Links)
					{
						B = OperandA->Read(Lk.Edge);
						if (!PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance)) { return false; }
					}
					return true;
				};
			}
		}

		// First, build the consolidated operand B
		switch (Adjacency.Consolidation)
		{
		case EPCGExAdjacencyGatherMode::Average:
			if (bCaptureFromNodes)
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = 0;
					for (const PCGExGraph::FLink Lk : Node.Links) { B += OperandB->Read(NodesRef[Lk.Node].PointIndex); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance);
				};
			}
			else
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = 0;
					for (const PCGExGraph::FLink Lk : Node.Links) { B += OperandB->Read(Lk.Edge); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance);
				};
			}

			break;
		case EPCGExAdjacencyGatherMode::Min:
			if (bCaptureFromNodes)
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = MAX_dbl;
					for (const PCGExGraph::FLink Lk : Node.Links) { B = FMath::Min(B, OperandB->Read(NodesRef[Lk.Node].PointIndex)); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance);
				};
			}
			else
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = MAX_dbl;
					for (const PCGExGraph::FLink Lk : Node.Links) { B = FMath::Min(B, OperandB->Read(Lk.Edge)); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance);
				};
			}
			break;
		case EPCGExAdjacencyGatherMode::Max:
			if (bCaptureFromNodes)
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = MIN_dbl_neg;
					for (const PCGExGraph::FLink Lk : Node.Links) { B = FMath::Max(B, OperandB->Read(NodesRef[Lk.Node].PointIndex)); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance);
				};
			}
			else
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = MIN_dbl_neg;
					for (const PCGExGraph::FLink Lk : Node.Links) { B = FMath::Max(B, OperandB->Read(Lk.Edge)); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance);
				};
			}
			break;
		case EPCGExAdjacencyGatherMode::Sum:
			if (bCaptureFromNodes)
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = MIN_dbl_neg;
					for (const PCGExGraph::FLink Lk : Node.Links) { B += FMath::Max(B, OperandB->Read(NodesRef[Lk.Node].PointIndex)); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance);
				};
			}
			else
			{
				PCGEX_SUB_TEST_FUNC
				{
					double B = MIN_dbl_neg;
					for (const PCGExGraph::FLink Lk : Node.Links) { B += FMath::Max(B, OperandB->Read(Lk.Edge)); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance);
				};
			}
			break;
		default:
		case EPCGExAdjacencyGatherMode::Individual:
			break;
		}
	}

	if (TestSubFunc) { return true; }

	if (bCaptureFromNodes)
	{
		PCGEX_SUB_TEST_FUNC
		{
			// Only some adjacent samples must pass the comparison
			const int32 Threshold = Adjacency.GetThreshold(Node);
			if (Threshold == -1) { return false; }
			int32 LocalSuccessCount = 0;
			double B = 0;

			for (const PCGExGraph::FLink Lk : Node.Links)
			{
				B = OperandA->Read(NodesRef[Lk.Node].PointIndex);
				if (PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance)) { LocalSuccessCount++; }
			}

			return PCGExCompare::Compare(Adjacency.ThresholdComparison, LocalSuccessCount, Threshold, Adjacency.ThresholdTolerance);
		};
	}
	else
	{
		PCGEX_SUB_TEST_FUNC
		{
			// Only some adjacent samples must pass the comparison
			const int32 Threshold = Adjacency.GetThreshold(Node);
			if (Threshold == -1) { return false; }
			int32 LocalSuccessCount = 0;
			double B = 0;

			for (const PCGExGraph::FLink Lk : Node.Links)
			{
				B = OperandA->Read(Lk.Edge);
				if (PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance)) { LocalSuccessCount++; }
			}

			return PCGExCompare::Compare(Adjacency.ThresholdComparison, LocalSuccessCount, Threshold, Adjacency.ThresholdTolerance);
		};
	}

#undef PCGEX_SUB_TEST_FUNC

	return true;
}

bool FNodeAdjacencyFilter::Test(const PCGExCluster::FNode& Node) const
{
	return TestSubFunc(Node, *Cluster->Nodes, OperandA->Read(Node.PointIndex));
}


PCGEX_CREATE_FILTER_FACTORY(NodeAdjacency)

#if WITH_EDITOR
FString UPCGExNodeAdjacencyFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = PCGEx::GetSelectorDisplayName(Config.OperandA) + PCGExCompare::ToString(Config.Comparison);

	DisplayName += PCGEx::GetSelectorDisplayName(Config.OperandB);
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
