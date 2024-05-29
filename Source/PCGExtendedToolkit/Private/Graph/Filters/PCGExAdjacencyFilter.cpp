// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/PCGExAdjacencyFilter.h"

#include "PCGPin.h"
#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExNodeAdjacencyFilter"
#define PCGEX_NAMESPACE NodeAdjacencyFilter

PCGExDataFilter::TFilter* UPCGExAdjacencyFilterFactory::CreateFilter() const
{
	return new PCGExNodeAdjacency::TAdjacencyFilter(this);
}

namespace PCGExNodeAdjacency
{
	void TAdjacencyFilter::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
	{
		bUseAbsoluteMeasure = TypedFilterFactory->MeasureType == EPCGExMeanMeasure::Absolute;
		bUseLocalMeasure = TypedFilterFactory->MeasureSource == EPCGExFetchType::Attribute;

		if (TypedFilterFactory->CompareAgainst == EPCGExOperandType::Attribute)
		{
			OperandA = new PCGEx::FLocalSingleFieldGetter();
			OperandA->Capture(TypedFilterFactory->OperandA);
			OperandA->Grab(*PointIO, false);

			bValid = OperandA->IsUsable(PointIO->GetNum());

			if (!bValid)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: {0}."), FText::FromName(TypedFilterFactory->OperandA.GetName())));
				PCGEX_DELETE(OperandA)
				return;
			}
		}

		if (bUseLocalMeasure)
		{
			LocalMeasure = new PCGEx::FLocalSingleFieldGetter();
			LocalMeasure->Capture(TypedFilterFactory->LocalMeasure);
			LocalMeasure->Grab(*PointIO, false);

			bValid = LocalMeasure->IsUsable(PointIO->GetNum());

			if (!bValid)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Local Measure attribute: {0}."), FText::FromName(TypedFilterFactory->LocalMeasure.GetName())));
				PCGEX_DELETE(OperandA)
				return;
			}
		}

		if (!bValid || TypedFilterFactory->OperandBSource != EPCGExGraphValueSource::Point) { return; }

		OperandB = new PCGEx::FLocalSingleFieldGetter();
		OperandB->Capture(TypedFilterFactory->OperandB);
		OperandB->Grab(*PointIO, false);
		bValid = OperandB->IsUsable(PointIO->GetNum());

		if (!bValid)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(TypedFilterFactory->OperandB.GetName())));
			PCGEX_DELETE(OperandB)
		}
	}

	void TAdjacencyFilter::CaptureEdges(const FPCGContext* InContext, const PCGExData::FPointIO* EdgeIO)
	{
		if (TypedFilterFactory->OperandBSource != EPCGExGraphValueSource::Edge) { return; }

		OperandB = new PCGEx::FLocalSingleFieldGetter();
		OperandB->Capture(TypedFilterFactory->OperandB);
		OperandB->Grab(*EdgeIO, false);
		bValid = OperandB->IsUsable(EdgeIO->GetNum());

		if (!bValid)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(TypedFilterFactory->OperandB.GetName())));
			PCGEX_DELETE(OperandB)
		}
	}

	void TAdjacencyFilter::PrepareForTesting(PCGExData::FPointIO* PointIO)
	{
		TClusterFilter::PrepareForTesting(PointIO);

		if (TypedFilterFactory->Mode == EPCGExAdjacencyTestMode::Some)
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
						CachedMeasure[i] = LocalMeasure->Values[Node.PointIndex] * Node.AdjacentNodes.Num();
					}
				}
			}
			else
			{
				if (bUseAbsoluteMeasure)
				{
					for (int i = 0; i < NumNodes; i++) { CachedMeasure[i] = TypedFilterFactory->ConstantMeasure; }
				}
				else
				{
					for (int i = 0; i < NumNodes; i++)
					{
						const PCGExCluster::FNode& Node = CapturedCluster->Nodes[i];
						CachedMeasure[i] = TypedFilterFactory->ConstantMeasure * Node.AdjacentNodes.Num();
					}
				}
			}
		}
	}

	bool TAdjacencyFilter::Test(const int32 PointIndex) const
	{
		const PCGExCluster::FNode& Node = CapturedCluster->Nodes[PointIndex];
		const double A = OperandA->Values[Node.PointIndex];
		double B = 0;

		if (TypedFilterFactory->Mode == EPCGExAdjacencyTestMode::All)
		{
			for (const int32 OtherNodeIndex : Node.AdjacentNodes)
			{
				B = OperandA->Values[CapturedCluster->Nodes[OtherNodeIndex].PointIndex];
				if (!PCGExCompare::Compare(TypedFilterFactory->Comparison, A, B, TypedFilterFactory->Tolerance)) { return false; }
			}

			return true;
		}

		const double MeasureReference = CachedMeasure[PointIndex];

		if (TypedFilterFactory->SubsetMode == EPCGExAdjacencySubsetMode::AtLeast && bUseAbsoluteMeasure)
		{
			if (Node.AdjacentNodes.Num() < MeasureReference) { return false; } // Early exit, not enough neighbors.
		}

		if (TypedFilterFactory->Consolidation == EPCGExAdjacencyGatherMode::Individual)
		{
			double LocalSuccessCount = 0;
			for (const int32 OtherNodeIndex : Node.AdjacentNodes)
			{
				B = OperandA->Values[CapturedCluster->Nodes[OtherNodeIndex].PointIndex];
				if (PCGExCompare::Compare(TypedFilterFactory->Comparison, A, B, TypedFilterFactory->Tolerance)) { LocalSuccessCount++; }
			}

			if (!bUseAbsoluteMeasure) { LocalSuccessCount /= static_cast<double>(Node.AdjacentNodes.Num()); }

			switch (TypedFilterFactory->SubsetMode)
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

		switch (TypedFilterFactory->Consolidation)
		{
		case EPCGExAdjacencyGatherMode::Average:
			for (const int32 OtherNodeIndex : Node.AdjacentNodes) { B += OperandB->Values[CapturedCluster->Nodes[OtherNodeIndex].PointIndex]; }
			B /= static_cast<double>(Node.AdjacentNodes.Num());
			break;
		case EPCGExAdjacencyGatherMode::Min:
			B = TNumericLimits<double>::Max();
			for (const int32 OtherNodeIndex : Node.AdjacentNodes) { B = FMath::Min(B, OperandB->Values[CapturedCluster->Nodes[OtherNodeIndex].PointIndex]); }
			break;
		case EPCGExAdjacencyGatherMode::Max:
			B = TNumericLimits<double>::Min();
			for (const int32 OtherNodeIndex : Node.AdjacentNodes) { B = FMath::Max(B, OperandB->Values[CapturedCluster->Nodes[OtherNodeIndex].PointIndex]); }
			break;
		case EPCGExAdjacencyGatherMode::Sum:
			for (const int32 OtherNodeIndex : Node.AdjacentNodes) { B += OperandB->Values[CapturedCluster->Nodes[OtherNodeIndex].PointIndex]; }
			break;
		default: ;
		}

		return PCGExCompare::Compare(TypedFilterFactory->Comparison, A, B, TypedFilterFactory->Tolerance);
	}
}

PCGEX_CREATE_FILTER_FACTORY(Adjacency)

#if WITH_EDITOR
FString UPCGExAdjacencyFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = Descriptor.OperandA.GetName().ToString() + PCGExCompare::ToString(Descriptor.Comparison);

	DisplayName += Descriptor.OperandB.GetName().ToString();
	DisplayName += TEXT(" (");

	switch (Descriptor.Mode)
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
