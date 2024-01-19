// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExPruneEdgesByLength.h"

#include "IPCGExDebug.h"
#include "Data/PCGExGraphParamsData.h"
#include "Sampling/PCGExSampling.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExPruneEdgesByLengthSettings::GetMainOutputInitMode() const { return bPruneIsolatedPoints ? PCGExData::EInit::NewOutput : PCGExData::EInit::DuplicateInput; }
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

	Context->MinClusterSize = Settings->bRemoveSmallClusters ? FMath::Max(1, Settings->MinClusterSize) : 1;
	Context->MaxClusterSize = Settings->bRemoveBigClusters ? FMath::Max(1, Settings->MaxClusterSize) : TNumericLimits<int32>::Max();

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
		Context->NodeIndicesMap.Empty();

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

		PCGExGraph::BuildIndexedEdges(*Context->CurrentEdges, Context->NodeIndicesMap, Context->IndexedEdges);

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

		if (Settings->Measure == EPCGExEdgeLengthMeasure::Relative)
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
		case EPCGExEdgeMeanMethod::Average:
			Context->ReferenceValue = SumEdgeLength / Context->EdgeLength.Num();
			break;
		case EPCGExEdgeMeanMethod::Median:
			Context->ReferenceValue = PCGExMath::GetMedian(Context->EdgeLength);
			break;
		case EPCGExEdgeMeanMethod::Fixed:
			Context->ReferenceValue = Settings->MeanValue;
			break;
		case EPCGExEdgeMeanMethod::ModeMin:
			Context->ReferenceValue = PCGExMath::GetMode(Context->EdgeLength, false, Settings->ModeTolerance);
			break;
		case EPCGExEdgeMeanMethod::ModeMax:
			Context->ReferenceValue = PCGExMath::GetMode(Context->EdgeLength, true, Settings->ModeTolerance);
			break;
		case EPCGExEdgeMeanMethod::Central:
			Context->ReferenceValue = MinEdgeLength + (MaxEdgeLength - MinEdgeLength) * 0.5;
			break;
		}

		Context->ReferenceMin = Settings->bPruneBelowMean ? Context->ReferenceValue - Settings->PruneBelow : 0;
		Context->ReferenceMax = Settings->bPruneAboveMean ? Context->ReferenceValue + Settings->PruneAbove : TNumericLimits<double>::Max();

		Context->SetState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		auto Initialize = [&]()
		{
			Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, 6, Context->CurrentEdges);
			if (Settings->bPruneIsolatedPoints) { Context->GraphBuilder->EnablePointsPruning(); }
		};

		auto InsertEdge = [&](int32 EdgeIndex)
		{
			if (!FMath::IsWithin(Context->EdgeLength[EdgeIndex], Context->ReferenceMin, Context->ReferenceMax)) { return; }
			Context->GraphBuilder->Graph->InsertEdge(Context->IndexedEdges[EdgeIndex]);
		};

		if (!Context->Process(Initialize, InsertEdge, Context->IndexedEdges.Num())) { return false; }

		Context->GraphBuilder->Compile(Context, Context->MinClusterSize, Context->MaxClusterSize);
		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		if (Context->GraphBuilder->bCompiledSuccessfully)
		{
			if (Settings->bWriteMean)
			{
				Context->GraphBuilder->EdgesIO->ForEach(
					[&](PCGExData::FPointIO& PointIO, int32 Index)
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
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
