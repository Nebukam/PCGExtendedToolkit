// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/Nodes/PCGExAdjacencyFilter.h"

#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExNodeAdjacencyFilter"
#define PCGEX_NAMESPACE NodeAdjacencyFilter

PCGExDataFilter::TFilter* UPCGExAdjacencyFilterFactory::CreateFilter() const
{
	return new PCGExNodeAdjacency::TAdjacencyFilter(this);
}

namespace PCGExNodeAdjacency
{
	PCGExDataFilter::EType TAdjacencyFilter::GetFilterType() const { return PCGExDataFilter::EType::ClusterNode; }

	void TAdjacencyFilter::CaptureCluster(const FPCGContext* InContext, const PCGExCluster::FCluster* InCluster)
	{
		bCaptureFromNodes = TypedFilterFactory->Descriptor.OperandBSource != EPCGExGraphValueSource::Edge;
		TClusterNodeFilter::CaptureCluster(InContext, InCluster);
	}

	void TAdjacencyFilter::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
	{
		TFilter::Capture(InContext, PointIO);

		auto ExitFail = [&]()
		{
			bValid = false;

			Adjacency.Cleanup();
			PCGEX_DELETE(OperandA)
			PCGEX_DELETE(OperandB)
		};

		if (TypedFilterFactory->Descriptor.CompareAgainst == EPCGExFetchType::Attribute)
		{
			OperandA = new PCGEx::FLocalSingleFieldGetter();
			OperandA->Capture(TypedFilterFactory->Descriptor.OperandA);
			OperandA->Grab(PointIO, false);

			if (!OperandA->Grab(PointIO, false))
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.OperandA.GetName())));
				return ExitFail();
			}
		}

		if (!Adjacency.Init(InContext, PointIO)) { return ExitFail(); }

		if (!bCaptureFromNodes) { return; }

		OperandB = new PCGEx::FLocalSingleFieldGetter();
		OperandB->Capture(TypedFilterFactory->Descriptor.OperandB);

		if (!OperandB->Grab(PointIO, false))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.OperandB.GetName())));
			ExitFail();
		}
	}

	void TAdjacencyFilter::CaptureEdges(const FPCGContext* InContext, const PCGExData::FPointIO* EdgeIO)
	{
		if (bCaptureFromNodes) { return; }

		OperandB = new PCGEx::FLocalSingleFieldGetter();
		OperandB->Capture(TypedFilterFactory->Descriptor.OperandB);
		OperandB->Grab(EdgeIO, false);
		bValid = OperandB->IsUsable(EdgeIO->GetNum());

		if (!bValid)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.OperandB.GetName())));
			Adjacency.Cleanup();
			PCGEX_DELETE(OperandA)
			PCGEX_DELETE(OperandB)
		}
	}

	bool TAdjacencyFilter::PrepareForTesting(const PCGExData::FPointIO* PointIO)
	{
		TClusterNodeFilter::PrepareForTesting(PointIO);

		if (!Adjacency.bTestAllNeighbors)
		{
			// TODO : Test whether or not it's more efficient to cache the thresholds first or not (would need to grab Adjacency first instead of soft grab)
			/*
			const int32 NumNodes = CapturedCluster->Nodes.Num();
			CachedThreshold.SetNumUninitialized(NumNodes);
			for (const PCGExCluster::FNode& Node : CapturedCluster->Nodes) { CachedThreshold[Node.NodeIndex] = Adjacency.GetThreshold(Node); }
			*/
		}

		return false;
	}

	bool TAdjacencyFilter::Test(const int32 PointIndex) const
	{
		const PCGExCluster::FNode& Node = CapturedCluster->Nodes[PointIndex];
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
						B = OperandA->Values[CapturedCluster->Nodes[PCGEx::H64A(AdjacencyHash)].PointIndex];
						if (!PCGExCompare::Compare(TypedFilterFactory->Descriptor.Comparison, A, B, TypedFilterFactory->Descriptor.Tolerance)) { return false; }
					}
				}
				else
				{
					for (const uint64 AdjacencyHash : Node.Adjacency)
					{
						B = OperandA->Values[CapturedCluster->Edges[PCGEx::H64B(AdjacencyHash)].PointIndex];
						if (!PCGExCompare::Compare(TypedFilterFactory->Descriptor.Comparison, A, B, TypedFilterFactory->Descriptor.Tolerance)) { return false; }
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
					for (const uint64 AdjacencyHash : Node.Adjacency) { B += OperandB->Values[CapturedCluster->Nodes[PCGEx::H64A(AdjacencyHash)].PointIndex]; }
				}
				else
				{
					for (const uint64 AdjacencyHash : Node.Adjacency) { B += OperandB->Values[CapturedCluster->Edges[PCGEx::H64B(AdjacencyHash)].PointIndex]; }
				}
				B /= Node.Adjacency.Num();
				break;
			case EPCGExAdjacencyGatherMode::Min:
				B = TNumericLimits<double>::Max();
				if (bCaptureFromNodes)
				{
					for (const uint64 AdjacencyHash : Node.Adjacency) { B = FMath::Min(B, OperandB->Values[CapturedCluster->Nodes[PCGEx::H64A(AdjacencyHash)].PointIndex]); }
				}
				else
				{
					for (const uint64 AdjacencyHash : Node.Adjacency) { B = FMath::Min(B, OperandB->Values[CapturedCluster->Edges[PCGEx::H64B(AdjacencyHash)].PointIndex]); }
				}
				break;
			case EPCGExAdjacencyGatherMode::Max:
				B = TNumericLimits<double>::Min();
				if (bCaptureFromNodes)
				{
					for (const uint64 AdjacencyHash : Node.Adjacency) { B = FMath::Max(B, OperandB->Values[CapturedCluster->Nodes[PCGEx::H64A(AdjacencyHash)].PointIndex]); }
				}
				else
				{
					for (const uint64 AdjacencyHash : Node.Adjacency) { B = FMath::Max(B, OperandB->Values[CapturedCluster->Edges[PCGEx::H64B(AdjacencyHash)].PointIndex]); }
				}
				break;
			case EPCGExAdjacencyGatherMode::Sum:
				if (bCaptureFromNodes)
				{
					for (const uint64 AdjacencyHash : Node.Adjacency) { B += FMath::Max(B, OperandB->Values[CapturedCluster->Nodes[PCGEx::H64A(AdjacencyHash)].PointIndex]); }
				}
				else
				{
					for (const uint64 AdjacencyHash : Node.Adjacency) { B += FMath::Max(B, OperandB->Values[CapturedCluster->Edges[PCGEx::H64B(AdjacencyHash)].PointIndex]); }
				}
				break;
			}

			return PCGExCompare::Compare(TypedFilterFactory->Descriptor.Comparison, A, B, TypedFilterFactory->Descriptor.Tolerance);
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
				B = OperandA->Values[CapturedCluster->Nodes[PCGEx::H64A(AdjacencyHash)].PointIndex];
				if (PCGExCompare::Compare(TypedFilterFactory->Descriptor.Comparison, A, B, TypedFilterFactory->Descriptor.Tolerance)) { LocalSuccessCount++; }
			}
		}
		else
		{
			for (const uint64 AdjacencyHash : Node.Adjacency)
			{
				B = OperandA->Values[CapturedCluster->Edges[PCGEx::H64B(AdjacencyHash)].PointIndex];
				if (PCGExCompare::Compare(TypedFilterFactory->Descriptor.Comparison, A, B, TypedFilterFactory->Descriptor.Tolerance)) { LocalSuccessCount++; }
			}
		}

		return PCGExCompare::Compare(Adjacency.ThresholdComparison, LocalSuccessCount, Threshold);
	}
}

PCGEX_CREATE_FILTER_FACTORY(Adjacency)

#if WITH_EDITOR
FString UPCGExAdjacencyFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = Descriptor.OperandA.GetName().ToString() + PCGExCompare::ToString(Descriptor.Comparison);

	DisplayName += Descriptor.OperandB.GetName().ToString();
	DisplayName += TEXT(" (");

	switch (Descriptor.Adjacency.Mode)
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
