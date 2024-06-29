// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "..\..\Public\Paths\PCGExPathToClusters.h"
#include "Graph/PCGExGraph.h"
#include "Data/Blending/PCGExCompoundBlender.h"
#include "Data/Blending/PCGExMetadataBlender.h"
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

#if WITH_EDITOR
void
UPCGExPathToClustersSettings::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGExData::EInit
UPCGExPathToClustersSettings::GetMainOutputInitMode() const
{
	return PCGExData::EInit::NoOutput;
}

FName
UPCGExPathToClustersSettings::GetMainOutputLabel() const
{
	return PCGExGraph::OutputVerticesLabel;
}

PCGEX_INITIALIZE_ELEMENT(PathToClusters)

FPCGExPathToClustersContext::~FPCGExPathToClustersContext()
{
	PCGEX_TERMINATE_ASYNC
	if (CompoundFacade)
	{
		PCGEX_DELETE(CompoundFacade->Source)
		PCGEX_DELETE(CompoundFacade)
	}

	PCGEX_DELETE(CompoundProcessor)
}

bool
FPCGExPathToClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext))
	{
		return false;
	}

	PCGEX_CONTEXT_AND_SETTINGS(PathToClusters)

	const_cast<UPCGExPathToClustersSettings*>(Settings)
		->EdgeEdgeIntersectionSettings.ComputeDot();

	Context->CompoundProcessor = new PCGExGraph::FCompoundProcessor(
		Context,
		Settings->PointPointIntersectionSettings,
		Settings->DefaultPointsBlendingSettings,
		Settings->DefaultEdgesBlendingSettings);

	if (Settings->bFindPointEdgeIntersections)
	{
		Context->CompoundProcessor->InitPointEdge(
			Settings->PointEdgeIntersectionSettings,
			Settings->bUseCustomPointEdgeBlending,
			&Settings->CustomPointEdgeBlendingSettings);
	}

	if (Settings->bFindEdgeEdgeIntersections)
	{
		Context->CompoundProcessor->InitEdgeEdge(
			Settings->EdgeEdgeIntersectionSettings,
			Settings->bUseCustomPointEdgeBlending,
			&Settings->CustomEdgeEdgeBlendingSettings);
	}

	return true;
}

bool
FPCGExPathToClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathToClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathToClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context))
		{
			return true;
		}

		if (Settings->bFusePaths)
		{
			PCGExData::FPointIO* CompoundPoints =
				new PCGExData::FPointIO(nullptr);
			Context->CompoundFacade = new PCGExData::FFacade(CompoundPoints);

			CompoundPoints->SetInfos(
				-1, Settings->GetMainOutputLabel(), nullptr);
			CompoundPoints->InitializeOutput<UPCGExClusterNodesData>(
				PCGExData::EInit::NewOutput);

			if (!Context->StartBatchProcessingPoints<
				PCGExPathToClusters::FFusingProcessorBatch>(
				[](const PCGExData::FPointIO* Entry)
				{
					return Entry->GetNum() >= 2;
				},
				[&](PCGExPathToClusters::FFusingProcessorBatch* NewBatch)
				{
					NewBatch->bInlineProcessing = true;
					NewBatch->PointPointIntersectionSettings = Settings->PointPointIntersectionSettings;
				},
				PCGExGraph::State_ProcessingCompound))
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

	if (!Context->ProcessPointsBatch())
	{
		return false;
	}

#pragma region Intersection management

	if (Settings->bFusePaths)
	{
		if (Context->IsState(PCGExGraph::State_ProcessingCompound))
		{
			const PCGExPathToClusters::FFusingProcessorBatch* FusingBatch =
				static_cast<PCGExPathToClusters::FFusingProcessorBatch*>(
					Context->MainBatch);

			Context->CompoundGraph = FusingBatch->CompoundGraph;

			Context->CompoundProcessor->StartProcessing(
				FusingBatch->CompoundGraph,
				Context->CompoundFacade,
				Settings->GraphBuilderSettings,
				[&](PCGExGraph::FGraphBuilder* GraphBuilder)
				{
					TArray<PCGExGraph::FUnsignedEdge> UniqueEdges;
					FusingBatch->CompoundGraph->GetUniqueEdges(UniqueEdges);
					FusingBatch->CompoundGraph->WriteMetadata(
						GraphBuilder->Graph->NodeMetadata);

					GraphBuilder->Graph->InsertEdges(
						UniqueEdges, -1); // TODO : valid IOIndex from CompoundGraph
					UniqueEdges.Empty();
				});
		}

		if (!Context->CompoundProcessor->Execute())
		{
			return false;
		}

		Context->Done();
	}

#pragma endregion

	if (Context->IsDone())
	{
		if (Settings->bFusePaths) { Context->CompoundFacade->Source->OutputTo(Context); }
		else { Context->OutputMainPoints(); }
	}

	return Context->TryComplete();
}

namespace PCGExPathToClusters
{
#pragma region NonFusing

	FNonFusingProcessor::~FNonFusingProcessor()
	{
		PCGEX_DELETE(GraphBuilder)
	}

	bool
	FNonFusingProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathToClusters)

		if (!FPointsProcessor::Process(AsyncManager))
		{
			return false;
		}

		GraphBuilder = new PCGExGraph::FGraphBuilder(
			PointIO, &Settings->GraphBuilderSettings, 2);

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

	void
	FNonFusingProcessor::CompleteWork()
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

	bool
	FFusingProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathToClusters)

		if (!FPointsProcessor::Process(AsyncManager))
		{
			return false;
		}

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		const int32 NumPoints = InPoints.Num();
		const int32 IOIndex = PointIO->IOIndex;

		if (NumPoints < 2)
		{
			return false;
		}

		for (int i = 0; i < NumPoints; i++)
		{
			PCGExGraph::FCompoundNode* CurrentVtx =
				CompoundGraph->InsertPointUnsafe(InPoints[i], IOIndex, i);

			if (const int32 PrevIndex = i - 1; InPoints.IsValidIndex(PrevIndex))
			{
				PCGExGraph::FCompoundNode* OtherVtx =
					CompoundGraph->InsertPoint(
						InPoints[PrevIndex], IOIndex, PrevIndex);

				CurrentVtx->Adjacency.Add(OtherVtx->Index);
				OtherVtx->Adjacency.Add(CurrentVtx->Index);
			}

			if (const int32 NextIndex = i + 1; InPoints.IsValidIndex(NextIndex))
			{
				PCGExGraph::FCompoundNode* OtherVtx =
					CompoundGraph->InsertPoint(
						InPoints[NextIndex], IOIndex, NextIndex);

				CurrentVtx->Adjacency.Add(OtherVtx->Index);
				OtherVtx->Adjacency.Add(CurrentVtx->Index);
			}
		}

		if (Settings->bClosedPath)
		{
			// Create an edge between first and last points
			const int32 LastIndex = NumPoints - 1;
			CompoundGraph->InsertEdge(
				InPoints[0], IOIndex, 0, InPoints[LastIndex], IOIndex, LastIndex);
		}

		return true;
	}

	FFusingProcessorBatch::FFusingProcessorBatch(
		FPCGContext* InContext,
		const TArray<PCGExData::FPointIO*>& InPointsCollection)
		: TBatch(InContext, InPointsCollection)
	{
		bInlineProcessing = true; // Points need to be added in the same order to
		// yield deterministic results
	}

	FFusingProcessorBatch::~FFusingProcessorBatch()
	{
		PCGEX_DELETE(CompoundGraph)
		PCGEX_DELETE(CompoundPointsBlender)
		CompoundPoints = nullptr;
	}

	void
	FFusingProcessorBatch::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathToClusters)

		MainPoints = TypedContext->MainPoints;

		FBox Bounds = FBox(ForceInit);
		for (const PCGExData::FPointIO* IO : PointsCollection)
		{
			Bounds += IO->GetIn()->GetBounds().ExpandBy(10);
		}

		CompoundGraph = new PCGExGraph::FCompoundGraph(
			Settings->PointPointIntersectionSettings.FuseSettings,
			Bounds,
			true,
			Settings->PointPointIntersectionSettings.FuseMethod);

		TBatch<FFusingProcessor>::Process(AsyncManager);
	}

	bool
	FFusingProcessorBatch::PrepareSingle(FFusingProcessor* PointsProcessor)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathToClusters)

		if (!TBatch<FFusingProcessor>::PrepareSingle(PointsProcessor))
		{
			return false;
		}

		PointsProcessor->CompoundGraph = CompoundGraph;

		return true;
	}

	void
	FFusingProcessorBatch::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathToClusters)

		CompoundPointsBlender = new PCGExDataBlending::FCompoundBlender(
			&Settings->DefaultPointsBlendingSettings);
		CompoundPointsBlender->AddSources(ProcessorFacades);

		CompoundPoints = TypedContext->CompoundFacade->Source;
		const int32 NumCompoundedNodes = CompoundGraph->NumNodes();
		CompoundPoints->InitializeNum(NumCompoundedNodes, true);

		CompoundPointsBlender->PrepareMerge(
			TypedContext->CompoundFacade, CompoundGraph->PointsCompounds);

		StartParallelLoopForRange(
			NumCompoundedNodes); // Update point center & blend

		TBatch<FFusingProcessor>::CompleteWork();
	}

	void
	FFusingProcessorBatch::ProcessSingleRangeIteration(const int32 Iteration)
	{
		// Update center
		CompoundPoints->GetMutablePoint(Iteration).Transform.SetLocation(
			CompoundGraph->Nodes[Iteration]->UpdateCenter(
				CompoundGraph->PointsCompounds, MainPoints));

		// Process data merging
		CompoundPointsBlender->MergeSingle(
			Iteration,
			PCGExSettings::GetDistanceSettings(PointPointIntersectionSettings));
	}

#pragma endregion
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
