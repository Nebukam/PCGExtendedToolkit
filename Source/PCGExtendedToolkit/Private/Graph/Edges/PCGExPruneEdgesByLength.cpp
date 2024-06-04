// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExPruneEdgesByLength.h"

#include "Data/PCGExGraphDefinition.h"
#include "Sampling/PCGExSampling.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExPruneEdgesByLengthSettings::GetMainOutputInitMode() const { return GraphBuilderSettings.bPruneIsolatedPoints ? PCGExData::EInit::NewOutput : PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExPruneEdgesByLengthSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

#pragma endregion

FPCGExPruneEdgesByLengthContext::~FPCGExPruneEdgesByLengthContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(GraphBuilder)

	IndexedEdges.Empty();
	EdgeLength.Empty();
}

PCGEX_INITIALIZE_ELEMENT(PruneEdgesByLength)

bool FPCGExPruneEdgesByLengthElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PruneEdgesByLength)
	PCGEX_OUTPUT_VALIDATE_NAME_NOWRITER(Mean)

	PCGEX_FWD(GraphBuilderSettings)

	return true;
}

bool FPCGExPruneEdgesByLengthElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPruneEdgesByLengthElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PruneEdgesByLength)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges) { return false; }
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		PCGEX_DELETE(Context->GraphBuilder)

		if (!Context->AdvanceEdges(false))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		Context->CurrentEdges->CreateInKeys();

		double MinEdgeLength = TNumericLimits<double>::Max();
		double MaxEdgeLength = TNumericLimits<double>::Min();
		double SumEdgeLength = 0;

		BuildIndexedEdges(*Context->CurrentEdges, Context->NodeIndicesMap, Context->IndexedEdges);

		const TArray<FPCGPoint>& InNodePoints = Context->CurrentIO->GetIn()->GetPoints();

		Context->EdgeLength.SetNum(Context->IndexedEdges.Num());

		for (const PCGExGraph::FIndexedEdge& Edge : Context->IndexedEdges)
		{
			const double EdgeLength = FVector::Dist(InNodePoints[Edge.Start].Transform.GetLocation(), InNodePoints[Edge.End].Transform.GetLocation());
			Context->EdgeLength[Edge.EdgeIndex] = EdgeLength;

			MinEdgeLength = FMath::Min(MinEdgeLength, EdgeLength);
			MaxEdgeLength = FMath::Max(MaxEdgeLength, EdgeLength);
			SumEdgeLength += EdgeLength;
		}

		if (Settings->Measure == EPCGExMeanMeasure::Relative)
		{
			double RelativeMinEdgeLength = TNumericLimits<double>::Max();
			double RelativeMaxEdgeLength = TNumericLimits<double>::Min();
			SumEdgeLength = 0;
			for (int i = 0; i < Context->EdgeLength.Num(); i++)
			{
				const double Normalized = (Context->EdgeLength[i] /= MaxEdgeLength);
				RelativeMinEdgeLength = FMath::Min(Normalized, RelativeMinEdgeLength);
				RelativeMaxEdgeLength = FMath::Max(Normalized, RelativeMaxEdgeLength);
				SumEdgeLength += Normalized;
			}
			MinEdgeLength = RelativeMinEdgeLength;
			MaxEdgeLength = 1;
		}

		switch (Settings->MeanMethod)
		{
		default:
		case EPCGExMeanMethod::Average:
			Context->ReferenceValue = SumEdgeLength / Context->EdgeLength.Num();
			break;
		case EPCGExMeanMethod::Median:
			Context->ReferenceValue = PCGExMath::GetMedian(Context->EdgeLength);
			break;
		case EPCGExMeanMethod::Fixed:
			Context->ReferenceValue = Settings->MeanValue;
			break;
		case EPCGExMeanMethod::ModeMin:
			Context->ReferenceValue = PCGExMath::GetMode(Context->EdgeLength, false, Settings->ModeTolerance);
			break;
		case EPCGExMeanMethod::ModeMax:
			Context->ReferenceValue = PCGExMath::GetMode(Context->EdgeLength, true, Settings->ModeTolerance);
			break;
		case EPCGExMeanMethod::Central:
			Context->ReferenceValue = MinEdgeLength + (MaxEdgeLength - MinEdgeLength) * 0.5;
			break;
		}

		const double RMin = Settings->bPruneBelowMean ? Context->ReferenceValue - Settings->PruneBelow : 0;
		const double RMax = Settings->bPruneAboveMean ? Context->ReferenceValue + Settings->PruneAbove : TNumericLimits<double>::Max();

		Context->ReferenceMin = FMath::Min(RMin, RMax);
		Context->ReferenceMax = FMath::Max(RMin, RMax);

		Context->SetState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, &Context->GraphBuilderSettings, 6, Context->MainEdges);

		for (PCGExGraph::FIndexedEdge& Edge : Context->IndexedEdges)
		{
			Edge.bValid = FMath::IsWithin(Context->EdgeLength[Edge.EdgeIndex], Context->ReferenceMin, Context->ReferenceMax);
		}

		Context->GraphBuilder->Graph->InsertEdges(Context->IndexedEdges);
		Context->GraphBuilder->Compile(Context);
		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		PCGEX_WAIT_ASYNC
		if (Context->GraphBuilder->bCompiledSuccessfully)
		{
			if (Settings->bWriteMean)
			{
				Context->GraphBuilder->EdgesIO->ForEach(
					[&](const PCGExData::FPointIO& PointIO, int32 Index)
					{
						PCGExData::WriteMark(PointIO.GetOut()->Metadata, Settings->MeanAttributeName, Context->ReferenceValue);
					});
			}
			Context->GraphBuilder->Write(Context);
		}
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputPointsAndEdges();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
