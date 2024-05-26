// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/PCGExNodeAdjacencyFilter.h"

#include "PCGPin.h"
#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExNodeAdjacencyFilter"
#define PCGEX_NAMESPACE NodeAdjacencyFilter

#if WITH_EDITOR
FString FPCGExAdjacencyFilterDescriptor::GetDisplayName() const
{
	FString DisplayName = OperandA.GetName().ToString() + PCGExCompare::ToString(Comparison);

	DisplayName += OperandB.GetName().ToString();
	DisplayName += TEXT(" (");

	switch (Mode)
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


PCGExDataFilter::TFilterHandler* UPCGExAdjacencyFilterDefinition::CreateHandler() const
{
	return new PCGExNodeAdjacency::TAdjacencyFilterHandler(this);
}

void UPCGExAdjacencyFilterDefinition::BeginDestroy()
{
	Super::BeginDestroy();
}

namespace PCGExNodeAdjacency
{
	void TAdjacencyFilterHandler::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
	{
		bUseAbsoluteMeasure = AdjacencyFilter->MeasureType == EPCGExMeanMeasure::Absolute;
		bUseLocalMeasure = AdjacencyFilter->MeasureSource == EPCGExFetchType::Attribute;

		if (AdjacencyFilter->CompareAgainst == EPCGExOperandType::Attribute)
		{
			OperandA = new PCGEx::FLocalSingleFieldGetter();
			OperandA->Capture(AdjacencyFilter->OperandA);
			OperandA->Grab(*PointIO, false);

			bValid = OperandA->IsUsable(PointIO->GetNum());

			if (!bValid)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: {0}."), FText::FromName(AdjacencyFilter->OperandA.GetName())));
				PCGEX_DELETE(OperandA)
				return;
			}
		}

		if (bUseLocalMeasure)
		{
			LocalMeasure = new PCGEx::FLocalSingleFieldGetter();
			LocalMeasure->Capture(AdjacencyFilter->LocalMeasure);
			LocalMeasure->Grab(*PointIO, false);

			bValid = LocalMeasure->IsUsable(PointIO->GetNum());

			if (!bValid)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Local Measure attribute: {0}."), FText::FromName(AdjacencyFilter->LocalMeasure.GetName())));
				PCGEX_DELETE(OperandA)
				return;
			}
		}

		if (!bValid || AdjacencyFilter->OperandBSource != EPCGExGraphValueSource::Point) { return; }

		OperandB = new PCGEx::FLocalSingleFieldGetter();
		OperandB->Capture(AdjacencyFilter->OperandB);
		OperandB->Grab(*PointIO, false);
		bValid = OperandB->IsUsable(PointIO->GetNum());

		if (!bValid)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(AdjacencyFilter->OperandB.GetName())));
			PCGEX_DELETE(OperandB)
		}
	}

	void TAdjacencyFilterHandler::CaptureEdges(const FPCGContext* InContext, const PCGExData::FPointIO* EdgeIO)
	{
		if (AdjacencyFilter->OperandBSource != EPCGExGraphValueSource::Edge) { return; }

		OperandB = new PCGEx::FLocalSingleFieldGetter();
		OperandB->Capture(AdjacencyFilter->OperandB);
		OperandB->Grab(*EdgeIO, false);
		bValid = OperandB->IsUsable(EdgeIO->GetNum());

		if (!bValid)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(AdjacencyFilter->OperandB.GetName())));
			PCGEX_DELETE(OperandB)
		}
	}

	void TAdjacencyFilterHandler::PrepareForTesting(PCGExData::FPointIO* PointIO)
	{
		TClusterFilterHandler::PrepareForTesting(PointIO);

		if (AdjacencyFilter->Mode == EPCGExAdjacencyTestMode::Some)
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
					for (int i = 0; i < NumNodes; i++) { CachedMeasure[i] = AdjacencyFilter->ConstantMeasure; }
				}
				else
				{
					for (int i = 0; i < NumNodes; i++)
					{
						const PCGExCluster::FNode& Node = CapturedCluster->Nodes[i];
						CachedMeasure[i] = AdjacencyFilter->ConstantMeasure * Node.AdjacentNodes.Num();
					}
				}
			}
		}
	}

	bool TAdjacencyFilterHandler::Test(const int32 PointIndex) const
	{
		const PCGExCluster::FNode& Node = CapturedCluster->Nodes[PointIndex];
		const double A = OperandA->Values[Node.PointIndex];
		double B = 0;

		if (AdjacencyFilter->Mode == EPCGExAdjacencyTestMode::All)
		{
			for (const int32 OtherNodeIndex : Node.AdjacentNodes)
			{
				B = OperandA->Values[CapturedCluster->Nodes[OtherNodeIndex].PointIndex];
				if (!PCGExCompare::Compare(AdjacencyFilter->Comparison, A, B, AdjacencyFilter->Tolerance)) { return false; }
			}

			return true;
		}

		const double MeasureReference = CachedMeasure[PointIndex];

		if (AdjacencyFilter->SubsetMode == EPCGExAdjacencySubsetMode::AtLeast && bUseAbsoluteMeasure)
		{
			if (Node.AdjacentNodes.Num() < MeasureReference) { return false; } // Early exit, not enough neighbors.
		}

		if (AdjacencyFilter->Consolidation == EPCGExAdjacencyGatherMode::Individual)
		{
			double LocalSuccessCount = 0;
			for (const int32 OtherNodeIndex : Node.AdjacentNodes)
			{
				B = OperandA->Values[CapturedCluster->Nodes[OtherNodeIndex].PointIndex];
				if (PCGExCompare::Compare(AdjacencyFilter->Comparison, A, B, AdjacencyFilter->Tolerance)) { LocalSuccessCount++; }
			}

			if (!bUseAbsoluteMeasure) { LocalSuccessCount /= static_cast<double>(Node.AdjacentNodes.Num()); }

			switch (AdjacencyFilter->SubsetMode)
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

		switch (AdjacencyFilter->Consolidation)
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

		return PCGExCompare::Compare(AdjacencyFilter->Comparison, A, B, AdjacencyFilter->Tolerance);
	}
}

FPCGElementPtr UPCGExNodeAdjacencyFilterSettings::CreateElement() const { return MakeShared<FPCGExNodeAdjacencyFilterElement>(); }

#if WITH_EDITOR
void UPCGExNodeAdjacencyFilterSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

bool FPCGExNodeAdjacencyFilterElement::ExecuteInternal(
	FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExNodeAdjacencyFilterElement::Execute);

	PCGEX_SETTINGS(NodeAdjacencyFilter)

	UPCGExAdjacencyFilterDefinition* OutTest = NewObject<UPCGExAdjacencyFilterDefinition>();
	OutTest->ApplyDescriptor(Settings->Descriptor);

	FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
	Output.Data = OutTest;
	Output.Pin = PCGExGraph::OutputSocketStateLabel;

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
