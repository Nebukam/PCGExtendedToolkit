// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathToClusters.h"


#include "Data/PCGExPointIO.h"
#include "Data/PCGExUnionData.h"
#include "Graph/PCGExGraph.h"
#include "Graph/Data/PCGExClusterData.h"
#include "Graph/PCGExUnionProcessor.h"
#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExPathToClustersElement"
#define PCGEX_NAMESPACE BuildCustomGraph

TArray<FPCGPinProperties>
UPCGExPathToClustersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(PathToClusters)

TSharedPtr<PCGExPointsMT::IBatch> FPCGExPathToClustersContext::CreatePointBatchInstance(const TArray<TWeakPtr<PCGExData::FPointIO>>& InData) const
{
	PCGEX_SETTINGS_LOCAL(PathToClusters)
	if (Settings->bFusePaths) { return MakeShared<PCGExPointsMT::TBatch<PCGExPathToClusters::FFusingProcessor>>(const_cast<FPCGExPathToClustersContext*>(this), InData); }
	return MakeShared<PCGExPointsMT::TBatch<PCGExPathToClusters::FNonFusingProcessor>>(const_cast<FPCGExPathToClustersContext*>(this), InData);
}

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

		// TODO : Support local fuse distance, requires access to all input facades
		if (!Context->UnionGraph->Init(Context)) { return false; }
		Context->UnionGraph->Reserve(Context->MainPoints->GetInNumPoints(), -1);

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
			if (!Context->StartBatchProcessingPoints(
				[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
				{
					if (Entry->GetNum() < 2)
					{
						bHasInvalidInputs = true;
						return false;
					}

					return true;
				},
				[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
				{
					NewBatch->bSkipCompletion = true;
					NewBatch->bForceSingleThreadedProcessing = Settings->PointPointIntersectionDetails.FuseDetails.DoInlineInsertion();
				}))
			{
				return Context->CancelExecution(TEXT("Could not build any clusters."));
			}
		}
		else
		{
			PCGEX_ON_INVALILD_INPUTS(FTEXT("Some input have less than 2 points and will be ignored."))
			if (!Context->StartBatchProcessingPoints(
				[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
				{
					if (Entry->GetNum() < 2)
					{
						bHasInvalidInputs = true;
						return false;
					}
					return true;
				},
				[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
				{
				}))
			{
				return Context->CancelExecution(TEXT("Could not build any clusters."));
			}
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(Settings->bFusePaths ? PCGExGraph::State_PreparingUnion : PCGExCommon::State_Done)

#pragma region Intersection management

	if (Settings->bFusePaths)
	{
		PCGEX_ON_STATE(PCGExGraph::State_PreparingUnion)
		{
			const int32 NumFacades = Context->MainBatch->ProcessorFacades.Num();
			Context->PathsFacades.Reserve(NumFacades);

			PCGExPointsMT::TBatch<PCGExPathToClusters::FFusingProcessor>* MainBatch = static_cast<PCGExPointsMT::TBatch<PCGExPathToClusters::FFusingProcessor>*>(Context->MainBatch.Get());

			for (int Pi = 0; Pi < MainBatch->GetNumProcessors(); Pi++)
			{
				const TSharedPtr<PCGExPointsMT::IProcessor> P = MainBatch->GetProcessor<PCGExPointsMT::IProcessor>(Pi);
				if (!P->bIsProcessorValid) { continue; }
				Context->PathsFacades.Add(P->PointDataFacade);
			}

			Context->MainBatch.Reset();

			if (!Context->UnionProcessor->StartExecution(Context->PathsFacades, Settings->GraphBuilderDetails)) { return true; }
		}

		if (!Context->UnionProcessor->Execute()) { return false; }

		Context->Done();
	}

#pragma endregion

	if (Settings->bFusePaths) { (void)Context->UnionDataFacade->Source->StageOutput(Context); }
	else { Context->MainPoints->StageOutputs(); }

	return Context->TryComplete();
}

namespace PCGExPathToClusters
{
#pragma region NonFusing

	FNonFusingProcessor::~FNonFusingProcessor()
	{
	}

	bool FNonFusingProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		if (!IProcessor::Process(InAsyncManager)) { return false; }

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		bClosedLoop = PCGExPaths::GetClosedLoop(PointIO->GetIn());

		GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(PointDataFacade, &Settings->GraphBuilderDetails);

		const int32 NumPoints = PointDataFacade->GetNum();

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
		PointDataFacade->WriteFastest(AsyncManager);
	}

#pragma endregion

#pragma region Fusing

	FFusingProcessor::~FFusingProcessor()
	{
	}

	bool FFusingProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		if (!IProcessor::Process(InAsyncManager)) { return false; }

		const int32 NumPoints = PointDataFacade->GetNum();

		IOIndex = PointDataFacade->Source->IOIndex;
		LastIndex = NumPoints - 1;

		if (NumPoints < 2) { return false; }

		UnionGraph = Context->UnionGraph;
		bClosedLoop = PCGExPaths::GetClosedLoop(PointDataFacade->GetIn());
		bForceSingleThreadedProcessPoints = Settings->PointPointIntersectionDetails.FuseDetails.DoInlineInsertion();

		if (bForceSingleThreadedProcessPoints)
		{
			// Blunt insert since processor don't have a "wait"
			InsertEdges(PCGExMT::FScope(0, NumPoints), true);
			//OnInsertionComplete();
		}
		else
		{
			PCGEX_ASYNC_GROUP_CHKD(AsyncManager, InsertEdges)

			InsertEdges->OnSubLoopStartCallback = [PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				This->InsertEdges(Scope, false);
			};

			InsertEdges->StartSubLoops(NumPoints, 256);
		}

		return true;
	}

	void FFusingProcessor::InsertEdges(const PCGExMT::FScope& Scope, const bool bUnsafe)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathToClusters::FFusingProcessor::InsertEdges);

		const UPCGBasePointData* InPointData = PointDataFacade->GetIn();

		if (bUnsafe)
		{
			PCGEX_SCOPE_LOOP(i)
			{
				const int32 NextIndex = i + 1;
				if (NextIndex > LastIndex)
				{
					if (bClosedLoop)
					{
						UnionGraph->InsertEdge_Unsafe(PointDataFacade->GetInPoint(LastIndex), PointDataFacade->GetInPoint(0));
					}
					return;
				}

				UnionGraph->InsertEdge_Unsafe(PointDataFacade->GetInPoint(i), PointDataFacade->GetInPoint(NextIndex));
			}
		}
		else
		{
			PCGEX_SCOPE_LOOP(i)
			{
				const int32 NextIndex = i + 1;

				if (NextIndex > LastIndex)
				{
					if (bClosedLoop)
					{
						UnionGraph->InsertEdge(PointDataFacade->GetInPoint(LastIndex), PointDataFacade->GetInPoint(0));
					}
					return;
				}

				UnionGraph->InsertEdge(PointDataFacade->GetInPoint(i), PointDataFacade->GetInPoint(NextIndex));
			}
		}
	}

#pragma endregion
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
