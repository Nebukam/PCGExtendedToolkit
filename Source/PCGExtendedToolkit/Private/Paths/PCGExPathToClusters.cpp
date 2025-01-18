// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathToClusters.h"
#include "Graph/PCGExGraph.h"
#include "Graph/Data/PCGExClusterData.h"
#include "Graph/PCGExUnionHelpers.h"

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

PCGExData::EIOInit UPCGExPathToClustersSettings::GetMainOutputInitMode() const
{
	return PCGExData::EIOInit::None;
}

PCGEX_INITIALIZE_ELEMENT(PathToClusters)

bool FPCGExPathToClustersElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathToClusters)

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

	const_cast<UPCGExPathToClustersSettings*>(Settings)->EdgeEdgeIntersectionDetails.Init();

	if (Settings->bFusePaths)
	{
		const TSharedPtr<PCGExData::FPointIO> UnionVtxPoints = PCGExData::NewPointIO(Context, Settings->GetMainOutputPin());
		UnionVtxPoints->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EIOInit::New);

		Context->UnionDataFacade = MakeShared<PCGExData::FFacade>(UnionVtxPoints.ToSharedRef());

		Context->UnionGraph = MakeShared<PCGExGraph::FUnionGraph>(
			Settings->PointPointIntersectionDetails.FuseDetails,
			Context->MainPoints->GetInBounds().ExpandBy(10));

		Context->UnionGraph->EdgesUnion->bIsAbstract = true; // Because we don't have edge data

		Context->UnionProcessor = MakeShared<PCGExGraph::FUnionProcessor>(
			Context,
			Context->UnionDataFacade.ToSharedRef(),
			Context->UnionGraph.ToSharedRef(),
			Settings->PointPointIntersectionDetails,
			Settings->DefaultPointsBlendingDetails,
			Settings->DefaultEdgesBlendingDetails);

		Context->UnionProcessor->VtxCarryOverDetails = &Context->CarryOverDetails;

		if (Settings->bFindPointEdgeIntersections)
		{
			Context->UnionProcessor->InitPointEdge(
				Settings->PointEdgeIntersectionDetails,
				Settings->bUseCustomPointEdgeBlending,
				&Settings->CustomPointEdgeBlendingDetails);
		}

		if (Settings->bFindEdgeEdgeIntersections)
		{
			Context->UnionProcessor->InitEdgeEdge(
				Settings->EdgeEdgeIntersectionDetails,
				Settings->bUseCustomPointEdgeBlending,
				&Settings->CustomEdgeEdgeBlendingDetails);
		}
	}


	return true;
}

bool FPCGExPathToClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathToClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathToClusters)
	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		if (Settings->bFusePaths)
		{
			PCGEX_ON_INVALILD_INPUTS(FTEXT("Some input have less than 2 points and will be ignored."))
			if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPathToClusters::FFusingProcessor>>(
				[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
				{
					if (Entry->GetNum() < 2)
					{
						bHasInvalidInputs = true;
						return false;
					}
					return true;
				},
				[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExPathToClusters::FFusingProcessor>>& NewBatch)
				{
					NewBatch->bSkipCompletion = true;
					NewBatch->bDaisyChainProcessing = Settings->PointPointIntersectionDetails.FuseDetails.DoInlineInsertion();
				}))
			{
				return Context->CancelExecution(TEXT("Could not build any clusters."));
			}
		}
		else
		{
			PCGEX_ON_INVALILD_INPUTS(FTEXT("Some input have less than 2 points and will be ignored."))
			if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPathToClusters::FNonFusingProcessor>>(
				[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
				{
					if (Entry->GetNum() < 2)
					{
						bHasInvalidInputs = true;
						return false;
					}
					return true;
				},
				[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExPathToClusters::FNonFusingProcessor>>& NewBatch)
				{
				}))
			{
				return Context->CancelExecution(TEXT("Could not build any clusters."));
			}
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(Settings->bFusePaths ? PCGExGraph::State_PreparingUnion : PCGEx::State_Done)

#pragma region Intersection management

	if (Settings->bFusePaths)
	{
		PCGEX_ON_STATE(PCGExGraph::State_PreparingUnion)
		{
			const int32 NumFacades = Context->MainBatch->ProcessorFacades.Num();
			Context->PathsFacades.Reserve(NumFacades);

			PCGExPointsMT::TBatch<PCGExPathToClusters::FFusingProcessor>* MainBatch = static_cast<PCGExPointsMT::TBatch<PCGExPathToClusters::FFusingProcessor>*>(Context->MainBatch.Get());
			for (const TSharedRef<PCGExPathToClusters::FFusingProcessor>& Processor : MainBatch->Processors)
			{
				if (!Processor->bIsProcessorValid) { continue; }
				Context->PathsFacades.Add(Processor->PointDataFacade);
			}

			Context->MainBatch.Reset();

			if (!Context->UnionProcessor->StartExecution(Context->PathsFacades, Settings->GraphBuilderDetails)) { return true; }
		}

		if (!Context->UnionProcessor->Execute()) { return false; }

		Context->Done();
	}

#pragma endregion

	if (Settings->bFusePaths) { Context->UnionDataFacade->Source->StageOutput(); }
	else { Context->MainPoints->StageOutputs(); }

	return Context->TryComplete();
}

namespace PCGExPathToClusters
{
#pragma region NonFusing

	FNonFusingProcessor::~FNonFusingProcessor()
	{
	}

	bool FNonFusingProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		bClosedLoop = Context->ClosedLoop.IsClosedLoop(PointIO);

		GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(PointDataFacade, &Settings->GraphBuilderDetails, 2);

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		const int32 NumPoints = InPoints.Num();

		PointIO->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EIOInit::New);

		TArray<PCGExGraph::FEdge> Edges;
		PCGEx::InitArray(Edges, bClosedLoop ? NumPoints : NumPoints - 1);

		for (int i = 0; i < Edges.Num(); i++)
		{
			Edges[i] = PCGExGraph::FEdge(i, i, i + 1, PointIO->IOIndex);
		}

		if (bClosedLoop)
		{
			const int32 LastIndex = Edges.Num() - 1;
			Edges[LastIndex] = PCGExGraph::FEdge(LastIndex, LastIndex, 0, PointIO->IOIndex);
		}

		GraphBuilder->Graph->InsertEdges(Edges);
		Edges.Empty();

		GraphBuilder->CompileAsync(AsyncManager, false);

		return true;
	}

	void FNonFusingProcessor::CompleteWork()
	{
		if (!GraphBuilder->bCompiledSuccessfully)
		{
			bIsProcessorValid = false;
			PCGEX_CLEAR_IO_VOID(PointDataFacade->Source)
			return;
		}

		GraphBuilder->StageEdgesOutputs();
		PointDataFacade->Write(AsyncManager);
	}

#pragma endregion

#pragma region Fusing

	FFusingProcessor::~FFusingProcessor()
	{
	}

	bool FFusingProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		InPoints = &PointDataFacade->GetIn()->GetPoints();
		const int32 NumPoints = InPoints->Num();
		IOIndex = PointDataFacade->Source->IOIndex;
		LastIndex = NumPoints - 1;

		if (NumPoints < 2) { return false; }

		UnionGraph = Context->UnionGraph;
		bClosedLoop = Context->ClosedLoop.IsClosedLoop(PointDataFacade->Source);
		bInlineProcessPoints = Settings->PointPointIntersectionDetails.FuseDetails.DoInlineInsertion();

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FFusingProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		const int32 NextIndex = Index + 1;
		if (NextIndex > LastIndex)
		{
			if (bClosedLoop)
			{
				UnionGraph->InsertEdge(
					*(InPoints->GetData() + LastIndex), IOIndex, LastIndex,
					*InPoints->GetData(), IOIndex, 0);
			}
			return;
		}
		UnionGraph->InsertEdge(
			*(InPoints->GetData() + Index), IOIndex, Index,
			*(InPoints->GetData() + NextIndex), IOIndex, NextIndex);
	}

	/*
	void FFusingProcessor::OnPointsProcessingComplete()
	{
		TPointsProcessor<FPCGExPathToClustersContext, UPCGExPathToClustersSettings>::OnPointsProcessingComplete();
		// Schedule next update early
		AsyncManager->DeferredResumeExecution([PCGEX_ASYNC_THIS_CAPTURE]()
		{
			// Hack to force daisy chain advance, since we know when we're ready to do so
			PCGEX_ASYNC_THIS
			This->Context->ProcessPointsBatch(PCGExGraph::State_PreparingUnion); // Force move to next
		});
	}
	*/

#pragma endregion
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
