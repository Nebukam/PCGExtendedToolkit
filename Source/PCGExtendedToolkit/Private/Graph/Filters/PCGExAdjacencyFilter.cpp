// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/PCGExAdjacencyFilter.h"

#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExNodeAdjacencyFilter"
#define PCGEX_NAMESPACE NodeAdjacencyFilter

PCGExDataFilter::TFilter* UPCGExAdjacencyFilterFactory::CreateFilter() const
{
	return new PCGExNodeAdjacency::TAdjacencyFilter(this);
}

namespace PCGExNodeAdjacency
{
	PCGExDataFilter::EType TAdjacencyFilter::GetFilterType() const { return PCGExDataFilter::EType::Cluster; }

	void TAdjacencyFilter::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
	{
		TFilter::Capture(InContext, PointIO);

		bUseAbsoluteMeasure = !TypedFilterFactory->Descriptor.Adjacency.IsRelativeMeasure();
		bUseLocalMeasure = TypedFilterFactory->Descriptor.Adjacency.IsLocalMeasure();

		if (TypedFilterFactory->Descriptor.CompareAgainst == EPCGExFetchType::Attribute)
		{
			OperandA = new PCGEx::FLocalSingleFieldGetter();
			OperandA->Capture(TypedFilterFactory->Descriptor.OperandA);
			OperandA->Grab(PointIO, false);

			bValid = OperandA->IsUsable(PointIO->GetNum());

			if (!bValid)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.OperandA.GetName())));
				PCGEX_DELETE(OperandA)
				return;
			}
		}

		if (bUseLocalMeasure)
		{
			LocalMeasure = new PCGEx::FLocalSingleFieldGetter();
			LocalMeasure->Capture(TypedFilterFactory->Descriptor.Adjacency.LocalMeasure);
			LocalMeasure->Grab(PointIO, false);

			bValid = LocalMeasure->IsUsable(PointIO->GetNum());

			if (!bValid)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Local Measure attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.Adjacency.LocalMeasure.GetName())));
				PCGEX_DELETE(OperandA)
				return;
			}
		}

		if (!bValid || TypedFilterFactory->Descriptor.OperandBSource != EPCGExGraphValueSource::Point) { return; }

		OperandB = new PCGEx::FLocalSingleFieldGetter();
		OperandB->Capture(TypedFilterFactory->Descriptor.OperandB);
		OperandB->Grab(PointIO, false);
		bValid = OperandB->IsUsable(PointIO->GetNum());

		if (!bValid)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.OperandB.GetName())));
			PCGEX_DELETE(OperandB)
		}
	}

	void TAdjacencyFilter::CaptureEdges(const FPCGContext* InContext, const PCGExData::FPointIO* EdgeIO)
	{
		if (TypedFilterFactory->Descriptor.OperandBSource != EPCGExGraphValueSource::Edge) { return; }

		OperandB = new PCGEx::FLocalSingleFieldGetter();
		OperandB->Capture(TypedFilterFactory->Descriptor.OperandB);
		OperandB->Grab(EdgeIO, false);
		bValid = OperandB->IsUsable(EdgeIO->GetNum());

		if (!bValid)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.OperandB.GetName())));
			PCGEX_DELETE(OperandB)
		}
	}

	bool TAdjacencyFilter::PrepareForTesting(const PCGExData::FPointIO* PointIO)
	{
		TClusterFilter::PrepareForTesting(PointIO);

		if (TypedFilterFactory->Descriptor.Adjacency.Mode == EPCGExAdjacencyTestMode::Some)
		{
			const int32 NumNodes = CapturedCluster->Nodes.Num();
			CachedMeasure.SetNumUninitialized(NumNodes);

			if (bUseLocalMeasure)
			{
				if (bUseAbsoluteMeasure)
				{
					for (int i = 0; i < NumNodes; i++) { CachedMeasure[i] = LocalMeasure->Values[CapturedCluster->Nodes[i].PointIndex]; }
				}
				else
				{
					for (int i = 0; i < NumNodes; i++)
					{
						const PCGExCluster::FNode& Node = CapturedCluster->Nodes[i];
						CachedMeasure[i] = LocalMeasure->Values[Node.PointIndex] * Node.Adjacency.Num();
					}
				}
			}
			else
			{
				if (bUseAbsoluteMeasure)
				{
					for (int i = 0; i < NumNodes; i++) { CachedMeasure[i] = TypedFilterFactory->Descriptor.Adjacency.ConstantMeasure; }
				}
				else
				{
					for (int i = 0; i < NumNodes; i++)
					{
						const PCGExCluster::FNode& Node = CapturedCluster->Nodes[i];
						CachedMeasure[i] = TypedFilterFactory->Descriptor.Adjacency.ConstantMeasure * Node.Adjacency.Num();
					}
				}
			}
		}

		return false;
	}

	bool TAdjacencyFilter::Test(const int32 PointIndex) const
	{
		const PCGExCluster::FNode& Node = CapturedCluster->Nodes[PointIndex];
		const double A = OperandA->Values[Node.PointIndex];
		double B = 0;

		if (TypedFilterFactory->Descriptor.Adjacency.Mode == EPCGExAdjacencyTestMode::All)
		{
			for (const uint64 AdjacencyHash : Node.Adjacency)
			{
				const uint32 OtherNodeIndex = PCGEx::H64A(AdjacencyHash);
				B = OperandA->Values[CapturedCluster->Nodes[OtherNodeIndex].PointIndex];
				if (!PCGExCompare::Compare(TypedFilterFactory->Descriptor.Comparison, A, B, TypedFilterFactory->Descriptor.Tolerance)) { return false; }
			}

			return true;
		}

		const double MeasureReference = CachedMeasure[PointIndex];

		if (TypedFilterFactory->Descriptor.Adjacency.SubsetMode == EPCGExAdjacencySubsetMode::AtLeast && bUseAbsoluteMeasure)
		{
			if (Node.Adjacency.Num() < MeasureReference) { return false; } // Early exit, not enough neighbors.
		}

		if (TypedFilterFactory->Descriptor.Adjacency.Consolidation == EPCGExAdjacencyGatherMode::Individual)
		{
			double LocalSuccessCount = 0;
			for (const uint64 AdjacencyHash : Node.Adjacency)
			{
				const uint32 OtherNodeIndex = PCGEx::H64A(AdjacencyHash);
				B = OperandA->Values[CapturedCluster->Nodes[OtherNodeIndex].PointIndex];
				if (PCGExCompare::Compare(TypedFilterFactory->Descriptor.Comparison, A, B, TypedFilterFactory->Descriptor.Tolerance)) { LocalSuccessCount++; }
			}

			if (!bUseAbsoluteMeasure) { LocalSuccessCount /= static_cast<double>(Node.Adjacency.Num()); }

			switch (TypedFilterFactory->Descriptor.Adjacency.SubsetMode)
			{
			case EPCGExAdjacencySubsetMode::AtLeast:
				return LocalSuccessCount >= MeasureReference;
			case EPCGExAdjacencySubsetMode::AtMost:
				return LocalSuccessCount <= MeasureReference;
			case EPCGExAdjacencySubsetMode::Exactly:
				return LocalSuccessCount == MeasureReference;
			default: return false;
			}
		}

		switch (TypedFilterFactory->Descriptor.Adjacency.Consolidation)
		{
		case EPCGExAdjacencyGatherMode::Average:
			for (const uint64 AdjacencyHash : Node.Adjacency) { B += OperandB->Values[CapturedCluster->Nodes[PCGEx::H64A(AdjacencyHash)].PointIndex]; }
			B /= static_cast<double>(Node.Adjacency.Num());
			break;
		case EPCGExAdjacencyGatherMode::Min:
			B = TNumericLimits<double>::Max();
			for (const uint64 AdjacencyHash : Node.Adjacency) { B = FMath::Min(B, OperandB->Values[CapturedCluster->Nodes[PCGEx::H64A(AdjacencyHash)].PointIndex]); }
			break;
		case EPCGExAdjacencyGatherMode::Max:
			B = TNumericLimits<double>::Min();
			for (const uint64 AdjacencyHash : Node.Adjacency) { B = FMath::Max(B, OperandB->Values[CapturedCluster->Nodes[PCGEx::H64A(AdjacencyHash)].PointIndex]); }
			break;
		case EPCGExAdjacencyGatherMode::Sum:
			for (const uint64 AdjacencyHash : Node.Adjacency) { B += OperandB->Values[CapturedCluster->Nodes[PCGEx::H64A(AdjacencyHash)].PointIndex]; }
			break;
		default: ;
		}

		return PCGExCompare::Compare(TypedFilterFactory->Descriptor.Comparison, A, B, TypedFilterFactory->Descriptor.Tolerance);
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
