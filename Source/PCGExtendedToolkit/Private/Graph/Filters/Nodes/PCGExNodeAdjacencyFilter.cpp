// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/Nodes/PCGExNodeAdjacencyFilter.h"


#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExNodeAdjacencyFilter"
#define PCGEX_NAMESPACE NodeAdjacencyFilter

void UPCGExNodeAdjacencyFilterFactory::GatherRequiredVtxAttributes(FPCGExContext* InContext, PCGExData::FReadableBufferConfigList& ReadableBufferConfigList) const
{
	Super::GatherRequiredVtxAttributes(InContext, ReadableBufferConfigList);
	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { ReadableBufferConfigList.Register<double>(InContext, Config.OperandA); }
	if (Config.OperandBSource == EPCGExClusterComponentSource::Vtx) { ReadableBufferConfigList.Register<double>(InContext, Config.OperandB); }
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

	if (Adjacency.bTestAllNeighbors)
	{
		// Each adjacent sample must pass the comparison, exit early.
		if (Adjacency.Consolidation == EPCGExAdjacencyGatherMode::Individual)
		{
			if (bCaptureFromNodes)
			{
				TestSubFunc = [&](const PCGExCluster::FNode& Node, const TArray<PCGExCluster::FNode>& NodesRef, const double A)
				{
					double B = 0;
					for (const uint64 AdjacencyHash : Node.Adjacency)
					{
						B = OperandA->Read(NodesRef[PCGEx::H64A(AdjacencyHash)].PointIndex);
						if (!PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance)) { return false; }
					}
					return true;
				};
			}
			else
			{
				TestSubFunc = [&](const PCGExCluster::FNode& Node, const TArray<PCGExCluster::FNode>& NodesRef, const double A)
				{
					double B = 0;
					for (const uint64 AdjacencyHash : Node.Adjacency)
					{
						B = OperandA->Read(PCGEx::H64B(AdjacencyHash));
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
				TestSubFunc = [&](const PCGExCluster::FNode& Node, const TArray<PCGExCluster::FNode>& NodesRef, const double A)
				{
					double B = 0;
					for (const uint64 AdjacencyHash : Node.Adjacency) { B += OperandB->Read(NodesRef[PCGEx::H64A(AdjacencyHash)].PointIndex); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B = Node.Adjacency.Num(), TypedFilterFactory->Config.Tolerance);
				};
			}
			else
			{
				TestSubFunc = [&](const PCGExCluster::FNode& Node, const TArray<PCGExCluster::FNode>& NodesRef, const double A)
				{
					double B = 0;
					for (const uint64 AdjacencyHash : Node.Adjacency) { B += OperandB->Read(PCGEx::H64B(AdjacencyHash)); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B = Node.Adjacency.Num(), TypedFilterFactory->Config.Tolerance);
				};
			}

			break;
		case EPCGExAdjacencyGatherMode::Min:
			if (bCaptureFromNodes)
			{
				TestSubFunc = [&](const PCGExCluster::FNode& Node, const TArray<PCGExCluster::FNode>& NodesRef, const double A)
				{
					double B = MAX_dbl;
					for (const uint64 AdjacencyHash : Node.Adjacency) { B = FMath::Min(B, OperandB->Read(NodesRef[PCGEx::H64A(AdjacencyHash)].PointIndex)); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B = Node.Adjacency.Num(), TypedFilterFactory->Config.Tolerance);
				};
			}
			else
			{
				TestSubFunc = [&](const PCGExCluster::FNode& Node, const TArray<PCGExCluster::FNode>& NodesRef, const double A)
				{
					double B = MAX_dbl;
					for (const uint64 AdjacencyHash : Node.Adjacency) { B = FMath::Min(B, OperandB->Read(PCGEx::H64B(AdjacencyHash))); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B = Node.Adjacency.Num(), TypedFilterFactory->Config.Tolerance);
				};
			}
			break;
		case EPCGExAdjacencyGatherMode::Max:
			if (bCaptureFromNodes)
			{
				TestSubFunc = [&](const PCGExCluster::FNode& Node, const TArray<PCGExCluster::FNode>& NodesRef, const double A)
				{
					double B = MIN_dbl;
					for (const uint64 AdjacencyHash : Node.Adjacency) { B = FMath::Max(B, OperandB->Read(NodesRef[PCGEx::H64A(AdjacencyHash)].PointIndex)); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B = Node.Adjacency.Num(), TypedFilterFactory->Config.Tolerance);
				};
			}
			else
			{
				TestSubFunc = [&](const PCGExCluster::FNode& Node, const TArray<PCGExCluster::FNode>& NodesRef, const double A)
				{
					double B = MIN_dbl;
					for (const uint64 AdjacencyHash : Node.Adjacency) { B = FMath::Max(B, OperandB->Read(PCGEx::H64B(AdjacencyHash))); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B = Node.Adjacency.Num(), TypedFilterFactory->Config.Tolerance);
				};
			}
			break;
		case EPCGExAdjacencyGatherMode::Sum:
			if (bCaptureFromNodes)
			{
				TestSubFunc = [&](const PCGExCluster::FNode& Node, const TArray<PCGExCluster::FNode>& NodesRef, const double A)
				{
					double B = MIN_dbl;
					for (const uint64 AdjacencyHash : Node.Adjacency) { B += FMath::Max(B, OperandB->Read(NodesRef[PCGEx::H64A(AdjacencyHash)].PointIndex)); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B = Node.Adjacency.Num(), TypedFilterFactory->Config.Tolerance);
				};
			}
			else
			{
				TestSubFunc = [&](const PCGExCluster::FNode& Node, const TArray<PCGExCluster::FNode>& NodesRef, const double A)
				{
					double B = MIN_dbl;
					for (const uint64 AdjacencyHash : Node.Adjacency) { B += FMath::Max(B, OperandB->Read(PCGEx::H64B(AdjacencyHash))); }
					return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B = Node.Adjacency.Num(), TypedFilterFactory->Config.Tolerance);
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
		TestSubFunc = [&](const PCGExCluster::FNode& Node, const TArray<PCGExCluster::FNode>& NodesRef, const double A)
		{
			// Only some adjacent samples must pass the comparison
			const int32 Threshold = Adjacency.GetThreshold(Node);
			if (Threshold == -1) { return false; }
			int32 LocalSuccessCount = 0;
			double B = 0;

			for (const uint64 AdjacencyHash : Node.Adjacency)
			{
				B = OperandA->Read(NodesRef[PCGEx::H64A(AdjacencyHash)].PointIndex);
				if (PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance)) { LocalSuccessCount++; }
			}

			return PCGExCompare::Compare(Adjacency.ThresholdComparison, LocalSuccessCount, Threshold, Adjacency.ThresholdTolerance);
		};
	}
	else
	{
		TestSubFunc = [&](const PCGExCluster::FNode& Node, const TArray<PCGExCluster::FNode>& NodesRef, const double A)
		{
			// Only some adjacent samples must pass the comparison
			const int32 Threshold = Adjacency.GetThreshold(Node);
			if (Threshold == -1) { return false; }
			int32 LocalSuccessCount = 0;
			double B = 0;

			for (const uint64 AdjacencyHash : Node.Adjacency)
			{
				B = OperandA->Read(PCGEx::H64B(AdjacencyHash));
				if (PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance)) { LocalSuccessCount++; }
			}

			return PCGExCompare::Compare(Adjacency.ThresholdComparison, LocalSuccessCount, Threshold, Adjacency.ThresholdTolerance);
		};
	}

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
