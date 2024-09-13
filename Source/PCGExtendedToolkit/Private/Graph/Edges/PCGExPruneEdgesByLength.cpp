// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExPruneEdgesByLength.h"


#include "Sampling/PCGExSampling.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExPruneEdgesByLengthSettings::GetMainOutputInitMode() const { return GraphBuilderDetails.bPruneIsolatedPoints ? PCGExData::EInit::NewOutput : PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExPruneEdgesByLengthSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

#pragma endregion

FPCGExPruneEdgesByLengthContext::~FPCGExPruneEdgesByLengthContext()
{
	PCGEX_TERMINATE_ASYNC
}

PCGEX_INITIALIZE_ELEMENT(PruneEdgesByLength)

bool FPCGExPruneEdgesByLengthElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PruneEdgesByLength)
	PCGEX_OUTPUT_VALIDATE_NAME_NOWRITER_C(Mean, double)

	PCGEX_FWD(GraphBuilderDetails)

	return true;
}

bool FPCGExPruneEdgesByLengthElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPruneEdgesByLengthElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PruneEdgesByLength)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatchWithGraphBuilder<PCGExPruneEdges::FProcessor>>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExClusterMT::TBatchWithGraphBuilder<PCGExPruneEdges::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any vtx/edge pairs."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExPruneEdges
{
	FProcessor::~FProcessor()
	{
		IndexedEdges.Empty();
		EdgeLengths.Empty();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPruneEdges::Process);
		PCGEX_SETTINGS(PruneEdgesByLength)

		AsyncManagerPtr = AsyncManager;

		//return FClusterProcessor::Process(AsyncManager); // Skip the cluster building process

		double MinEdgeLength = TNumericLimits<double>::Max();
		double MaxEdgeLength = TNumericLimits<double>::Min();
		double SumEdgeLength = 0;

		BuildIndexedEdges(EdgesIO, *EndpointsLookup, IndexedEdges);

		const TArray<FPCGPoint>& InNodePoints = VtxIO->GetIn()->GetPoints();

		EdgeLengths.SetNum(IndexedEdges.Num());

		for (const PCGExGraph::FIndexedEdge& Edge : IndexedEdges)
		{
			const double EdgeLength = FVector::Dist(InNodePoints[Edge.Start].Transform.GetLocation(), InNodePoints[Edge.End].Transform.GetLocation());
			EdgeLengths[Edge.EdgeIndex] = EdgeLength;

			MinEdgeLength = FMath::Min(MinEdgeLength, EdgeLength);
			MaxEdgeLength = FMath::Max(MaxEdgeLength, EdgeLength);
			SumEdgeLength += EdgeLength;
		}

		if (Settings->Measure == EPCGExMeanMeasure::Relative)
		{
			double RelativeMinEdgeLength = TNumericLimits<double>::Max();
			double RelativeMaxEdgeLength = TNumericLimits<double>::Min();
			SumEdgeLength = 0;
			for (int i = 0; i < EdgeLengths.Num(); ++i)
			{
				const double Normalized = (EdgeLengths[i] /= MaxEdgeLength);
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
			ReferenceValue = SumEdgeLength / EdgeLengths.Num();
			break;
		case EPCGExMeanMethod::Median:
			ReferenceValue = PCGExMath::GetMedian(EdgeLengths);
			break;
		case EPCGExMeanMethod::Fixed:
			ReferenceValue = Settings->MeanValue;
			break;
		case EPCGExMeanMethod::ModeMin:
			ReferenceValue = PCGExMath::GetMode(EdgeLengths, false, Settings->ModeTolerance);
			break;
		case EPCGExMeanMethod::ModeMax:
			ReferenceValue = PCGExMath::GetMode(EdgeLengths, true, Settings->ModeTolerance);
			break;
		case EPCGExMeanMethod::Central:
			ReferenceValue = MinEdgeLength + (MaxEdgeLength - MinEdgeLength) * 0.5;
			break;
		}

		const double RMin = Settings->bPruneBelowMean ? ReferenceValue - Settings->PruneBelow : 0;
		const double RMax = Settings->bPruneAboveMean ? ReferenceValue + Settings->PruneAbove : TNumericLimits<double>::Max();

		ReferenceMin = FMath::Min(RMin, RMax);
		ReferenceMax = FMath::Max(RMin, RMax);

		StartParallelLoopForRange(IndexedEdges.Num());

		return true;
	}

	void FProcessor::ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count)
	{
		Edge.bValid = FMath::IsWithin(EdgeLengths[Edge.EdgeIndex], ReferenceMin, ReferenceMax);
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 Count) { ProcessSingleEdge(Iteration, IndexedEdges[Iteration], LoopIdx, Count); }

	void FProcessor::CompleteWork()
	{
		TArray<PCGExGraph::FIndexedEdge> ValidEdges;
		for (PCGExGraph::FIndexedEdge& Edge : IndexedEdges) { if (Edge.bValid) { ValidEdges.Add(Edge); } }

		GraphBuilder->Graph->InsertEdges(ValidEdges);
	}
}
#undef LOCTEXT_NAMESPACE
