// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathToClusters.h"
#include "Graph/PCGExGraph.h"
#include "Data/Blending/PCGExCompoundBlender.h"
#include "Graph/Data/PCGExClusterData.h"
#include "Graph/PCGExCompoundHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExPathToClustersElement"
#define PCGEX_NAMESPACE BuildCustomGraph

TArray<FPCGPinProperties>
UPCGExPathToClustersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(
		PCGExGraph::OutputEdgesLabel,
		"Point data representing edges.",
		Required,
		{})
	return PinProperties;
}

PCGExData::EInit UPCGExPathToClustersSettings::GetMainOutputInitMode() const
{
	return PCGExData::EInit::NoOutput;
}

PCGEX_INITIALIZE_ELEMENT(PathToClusters)

FPCGExPathToClustersContext::~FPCGExPathToClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE_TARRAY(PathsFacades)

	PCGEX_DELETE_FACADE_AND_SOURCE(CompoundFacade)

	PCGEX_DELETE(CompoundProcessor)
}

bool FPCGExPathToClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathToClusters)

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

	const_cast<UPCGExPathToClustersSettings*>(Settings)
		->EdgeEdgeIntersectionDetails.ComputeDot();

	Context->CompoundProcessor = new PCGExGraph::FCompoundProcessor(
		Context,
		Settings->PointPointIntersectionDetails,
		Settings->DefaultPointsBlendingDetails,
		Settings->DefaultEdgesBlendingDetails);

	if (Settings->bFindPointEdgeIntersections)
	{
		Context->CompoundProcessor->InitPointEdge(
			Settings->PointEdgeIntersectionDetails,
			Settings->bUseCustomPointEdgeBlending,
			&Settings->CustomPointEdgeBlendingDetails);
	}

	if (Settings->bFindEdgeEdgeIntersections)
	{
		Context->CompoundProcessor->InitEdgeEdge(
			Settings->EdgeEdgeIntersectionDetails,
			Settings->bUseCustomPointEdgeBlending,
			&Settings->CustomEdgeEdgeBlendingDetails);
	}

	if (Settings->bFusePaths)
	{
		PCGExData::FPointIO* CompoundPoints = new PCGExData::FPointIO();
		CompoundPoints->SetInfos(-1, Settings->GetMainOutputLabel());
		CompoundPoints->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EInit::NewOutput);

		Context->CompoundFacade = new PCGExData::FFacade(CompoundPoints);

		Context->CompoundGraph = new PCGExGraph::FCompoundGraph(
			Settings->PointPointIntersectionDetails.FuseDetails,
			Context->MainPoints->GetInBounds().ExpandBy(10));
	}

	return true;
}

bool FPCGExPathToClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathToClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathToClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (Settings->bFusePaths)
		{
			if (!Context->StartBatchProcessingPoints<
				PCGExPointsMT::TBatch<PCGExPathToClusters::FFusingProcessor>>(
				[](const PCGExData::FPointIO* Entry)
				{
					return Entry->GetNum() >= 2;
				},
				[&](PCGExPointsMT::TBatch<PCGExPathToClusters::FFusingProcessor>* NewBatch)
				{
					NewBatch->bInlineProcessing = Settings->PointPointIntersectionDetails.FuseDetails.DoInlineInsertion();
				},
				PCGExGraph::State_PreparingCompound))
			{
				PCGE_LOG(
					Warning, GraphAndLog, FTEXT("Could not build any clusters."));
				return true;
			}
		}
		else
		{
			if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<
				PCGExPathToClusters::FNonFusingProcessor>>(
				[](const PCGExData::FPointIO* Entry)
				{
					return Entry->GetNum() >= 2;
				},
				[&](PCGExPointsMT::TBatch<
				PCGExPathToClusters::FNonFusingProcessor>* NewBatch)
				{
				},
				PCGExMT::State_Done))
			{
				PCGE_LOG(
					Warning, GraphAndLog, FTEXT("Could not build any clusters."));
				return true;
			}
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

#pragma region Intersection management

	if (Settings->bFusePaths)
	{
		if (Context->IsState(PCGExGraph::State_PreparingCompound))
		{
			Context->PathsFacades.Reserve(Context->MainBatch->ProcessorFacades.Num());
			Context->PathsFacades.Append(Context->MainBatch->ProcessorFacades);

			PCGExPointsMT::TBatch<PCGExPathToClusters::FFusingProcessor>* MainBatch = static_cast<PCGExPointsMT::TBatch<PCGExPathToClusters::FFusingProcessor>*>(Context->MainBatch);
			for (PCGExPathToClusters::FFusingProcessor* Processor : MainBatch->Processors)
			{
				Processor->PointDataFacade = nullptr; // Remove ownership of facade
				// before deleting the processor
			}

			PCGEX_DELETE(Context->MainBatch);

			if (!Context->CompoundProcessor->StartExecution(
				Context->CompoundGraph,
				Context->CompoundFacade,
				Context->PathsFacades,
				Settings->GraphBuilderDetails,
				&Settings->CarryOverDetails)) { return true; }
		}

		if (!Context->CompoundProcessor->Execute()) { return false; }

		Context->Done();
	}

#pragma endregion

	if (Settings->bFusePaths) { Context->CompoundFacade->Source->OutputTo(Context); }
	else { Context->OutputMainPoints(); }

	return Context->TryComplete();
}

namespace PCGExPathToClusters
{
#pragma region NonFusing

	FNonFusingProcessor::~FNonFusingProcessor()
	{
		PCGEX_DELETE(GraphBuilder)
	}

	bool FNonFusingProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathToClusters)

		if (!FPointsProcessor::Process(AsyncManager))
		{
			return false;
		}

		GraphBuilder = new PCGExGraph::FGraphBuilder(
			PointIO, &Settings->GraphBuilderDetails, 2);

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		const int32 NumPoints = InPoints.Num();

		PointIO->InitializeOutput<UPCGExClusterNodesData>(
			PCGExData::EInit::NewOutput);

		TArray<PCGExGraph::FIndexedEdge> Edges;

		Edges.SetNumUninitialized(
			Settings->bClosedPath ? NumPoints : NumPoints - 1);

		for (int i = 0; i < Edges.Num(); i++)
		{
			Edges[i] = PCGExGraph::FIndexedEdge(i, i, i + 1, PointIO->IOIndex);
		}

		if (Settings->bClosedPath)
		{
			const int32 LastIndex = Edges.Num() - 1;
			Edges[LastIndex] =
				PCGExGraph::FIndexedEdge(LastIndex, LastIndex, 0, PointIO->IOIndex);
		}

		GraphBuilder->Graph->InsertEdges(Edges);
		Edges.Empty();

		GraphBuilder->CompileAsync(AsyncManagerPtr);

		return true;
	}

	void FNonFusingProcessor::CompleteWork()
	{
		if (!GraphBuilder->bCompiledSuccessfully)
		{
			PointIO->InitializeOutput(PCGExData::EInit::NoOutput);
			return;
		}

		GraphBuilder->Write(Context);
	}

#pragma endregion

#pragma region Fusing

	FFusingProcessor::~FFusingProcessor()
	{
	}

	bool FFusingProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathToClusters)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		InPoints = &PointIO->GetIn()->GetPoints();
		const int32 NumPoints = InPoints->Num();
		IOIndex = PointIO->IOIndex;
		LastIndex = NumPoints - 1;

		if (NumPoints < 2) { return false; }

		CompoundGraph = TypedContext->CompoundGraph;
		bClosedPath = Settings->bClosedPath;
		bInlineProcessPoints = Settings->PointPointIntersectionDetails.FuseDetails.DoInlineInsertion();

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FFusingProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		const int32 NextIndex = Index + 1;
		if (NextIndex > LastIndex)
		{
			if (bClosedPath) { CompoundGraph->InsertEdge(*(InPoints->GetData() + LastIndex), IOIndex, LastIndex, *InPoints->GetData(), IOIndex, 0); }
			return;
		}
		CompoundGraph->InsertEdge(*(InPoints->GetData() + Index), IOIndex, Index, *(InPoints->GetData() + NextIndex), IOIndex, NextIndex);
	}

#pragma endregion
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
