// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathToEdgeClusters.h"
#include "Graph/PCGExGraph.h"
#include "Data/Blending/PCGExCompoundBlender.h"
#include "Data/Blending/PCGExMetadataBlender.h"

#define LOCTEXT_NAMESPACE "PCGExPathToEdgeClustersElement"
#define PCGEX_NAMESPACE BuildCustomGraph

UPCGExPathToEdgeClustersSettings::UPCGExPathToEdgeClustersSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPCGExPathToEdgeClustersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	return PinProperties;
}

#if WITH_EDITOR
void UPCGExPathToEdgeClustersSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGExData::EInit UPCGExPathToEdgeClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FName UPCGExPathToEdgeClustersSettings::GetMainInputLabel() const { return PCGExGraph::SourcePathsLabel; }

FName UPCGExPathToEdgeClustersSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(PathToEdgeClusters)

FPCGExPathToEdgeClustersContext::~FPCGExPathToEdgeClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(PointEdgeIntersections)
	PCGEX_DELETE(EdgeEdgeIntersections)

	PCGEX_DELETE(MetadataBlender)
}

bool FPCGExPathToEdgeClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathToEdgeClusters)

	const_cast<UPCGExPathToEdgeClustersSettings*>(Settings)->EdgeEdgeIntersectionSettings.ComputeDot();

	Context->GraphMetadataSettings.Grab(Context, Settings->PointPointIntersectionSettings);
	Context->GraphMetadataSettings.Grab(Context, Settings->PointEdgeIntersectionSettings);
	Context->GraphMetadataSettings.Grab(Context, Settings->EdgeEdgeIntersectionSettings);

	PCGEX_FWD(GraphBuilderSettings)

	return true;
}

// TODO : Use FPointProcessor + batch to either batch points into a massive compound graph
// or to generate individual graphs using a graph builder
// in one case, the graph builder will be owned by the processor
// in the other, the graph builder will be owned by the batch

bool FPCGExPathToEdgeClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathToEdgeClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathToEdgeClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (Settings->bFusePaths)
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			if (!Context->StartBatchProcessingPoints<PCGExPathToClusters::FFusingProcessorBatch>(
				[](const PCGExData::FPointIO* Entry) { return Entry->GetNum() >= 2; },
				[&](PCGExPathToClusters::FFusingProcessorBatch* NewBatch)
				{
					NewBatch->GraphBuilderSettings = Settings->GraphBuilderSettings;
					NewBatch->PointPointIntersectionSettings = Settings->PointPointIntersectionSettings;
				},
				PCGExGraph::State_ProcessingGraph))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
				return true;
			}
		}
		else
		{
			if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPathToClusters::FNonFusingProcessor>>(
				[](const PCGExData::FPointIO* Entry) { return Entry->GetNum() >= 2; },
				[&](PCGExPointsMT::TBatch<PCGExPathToClusters::FNonFusingProcessor>* NewBatch) { return; },
				PCGExMT::State_Done))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
				return true;
			}
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

#pragma region Intersection management
	
	auto FindPointEdgeIntersections = [&]()
	{
		Context->PointEdgeIntersections = new PCGExGraph::FPointEdgeIntersections(
			Context->GraphBuilder->Graph, Context->CompoundGraph, Context->CompoundPoints, Settings->PointEdgeIntersectionSettings);
		Context->SetState(PCGExGraph::State_FindingPointEdgeIntersections);
	};

	auto FindEdgeEdgeIntersections = [&]()
	{
		Context->EdgeEdgeIntersections = new PCGExGraph::FEdgeEdgeIntersections(
			Context->GraphBuilder->Graph, Context->CompoundGraph, Context->CompoundPoints, Settings->EdgeEdgeIntersectionSettings);
		Context->SetState(PCGExGraph::State_FindingEdgeEdgeIntersections);
	};

	if (Context->IsState(PCGExGraph::State_ProcessingGraph))
	{
		// At this point, the processors have completed & merged the compound graph

		const PCGExPathToClusters::FFusingProcessorBatch* FusingBatch = static_cast<PCGExPathToClusters::FFusingProcessorBatch*>(Context->MainBatch);

		Context->CompoundGraph = FusingBatch->CompoundGraph;
		Context->CompoundPoints = FusingBatch->CompoundPoints;
		Context->GraphBuilder = FusingBatch->GraphBuilder;

		Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CompoundPoints, &Context->GraphBuilderSettings, 4);

		TArray<PCGExGraph::FUnsignedEdge> UniqueEdges;
		FusingBatch->CompoundGraph->GetUniqueEdges(UniqueEdges);
		FusingBatch->CompoundGraph->WriteMetadata(Context->GraphBuilder->Graph->NodeMetadata);

		Context->GraphBuilder->Graph->InsertEdges(UniqueEdges, -1); //TODO : valid IOIndex from CompoundGraph
		UniqueEdges.Empty();

		//

		if (Settings->bFindPointEdgeIntersections) { FindPointEdgeIntersections(); }
		else if (Settings->bFindEdgeEdgeIntersections) { FindEdgeEdgeIntersections(); }
		else { Context->SetState(PCGExGraph::State_WritingClusters); }
	}

	if (Context->IsState(PCGExGraph::State_FindingPointEdgeIntersections))
	{
		auto PointEdge = [&](const int32 EdgeIndex)
		{
			const PCGExGraph::FIndexedEdge& Edge = Context->GraphBuilder->Graph->Edges[EdgeIndex];
			if (!Edge.bValid) { return; }
			FindCollinearNodes(Context->PointEdgeIntersections, EdgeIndex, Context->CompoundPoints->GetOut());
		};

		if (!Context->Process(PointEdge, Context->GraphBuilder->Graph->Edges.Num())) { return false; }

		Context->PointEdgeIntersections->Insert(); // TODO : Async?
		Context->CompoundPoints->CleanupKeys();    // Required for later blending as point count has changed

		Context->SetState(PCGExGraph::State_BlendingPointEdgeCrossings);
	}

	if (Context->IsState(PCGExGraph::State_BlendingPointEdgeCrossings))
	{
		auto Initialize = [&]()
		{
			if (Settings->bUseCustomPointEdgeBlending) { Context->MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(&Settings->CustomPointEdgeBlendingSettings)); }
			else { Context->MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(&Settings->DefaultPointsBlendingSettings)); }

			Context->MetadataBlender->PrepareForData(*Context->CompoundPoints, PCGExData::ESource::Out, true);
		};

		auto BlendPointEdgeMetadata = [&](const int32 Index)
		{
		};

		if (!Context->Process(Initialize, BlendPointEdgeMetadata, Context->PointEdgeIntersections->Edges.Num())) { return false; }

		if (Context->MetadataBlender) { Context->MetadataBlender->Write(); }

		PCGEX_DELETE(Context->PointEdgeIntersections)
		PCGEX_DELETE(Context->MetadataBlender)

		if (Settings->bFindEdgeEdgeIntersections) { FindEdgeEdgeIntersections(); }
		else { Context->SetState(PCGExGraph::State_WritingClusters); }
	}

	if (Context->IsState(PCGExGraph::State_FindingEdgeEdgeIntersections))
	{
		auto EdgeEdge = [&](const int32 EdgeIndex)
		{
			const PCGExGraph::FIndexedEdge& Edge = Context->GraphBuilder->Graph->Edges[EdgeIndex];
			if (!Edge.bValid) { return; }
			FindOverlappingEdges(Context->EdgeEdgeIntersections, EdgeIndex);
		};

		if (!Context->Process(EdgeEdge, Context->GraphBuilder->Graph->Edges.Num())) { return false; }

		Context->EdgeEdgeIntersections->Insert(); // TODO : Async?
		Context->CompoundPoints->CleanupKeys();   // Required for later blending as point count has changed

		Context->SetState(PCGExGraph::State_BlendingEdgeEdgeCrossings);
	}

	if (Context->IsState(PCGExGraph::State_BlendingEdgeEdgeCrossings))
	{
		auto Initialize = [&]()
		{
			if (Settings->bUseCustomPointEdgeBlending) { Context->MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(&Settings->CustomEdgeEdgeBlendingSettings)); }
			else { Context->MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(&Settings->DefaultPointsBlendingSettings)); }

			Context->MetadataBlender->PrepareForData(*Context->CompoundPoints, PCGExData::ESource::Out, true);
		};

		auto BlendCrossingMetadata = [&](const int32 Index) { Context->EdgeEdgeIntersections->BlendIntersection(Index, Context->MetadataBlender); };

		if (!Context->Process(Initialize, BlendCrossingMetadata, Context->EdgeEdgeIntersections->Crossings.Num())) { return false; }

		if (Context->MetadataBlender) { Context->MetadataBlender->Write(); }

		PCGEX_DELETE(Context->EdgeEdgeIntersections)
		PCGEX_DELETE(Context->MetadataBlender)

		Context->SetState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		Context->GraphBuilder->Compile(Context->GetAsyncManager(), &Context->GraphMetadataSettings);
		Context->SetAsyncState(PCGExGraph::State_WaitingOnWritingClusters);
		return false;
	}

	if (Context->IsState(PCGExGraph::State_WaitingOnWritingClusters))
	{
		PCGEX_WAIT_ASYNC

		if (Context->GraphBuilder->bCompiledSuccessfully)
		{
			if (Settings->bFusePaths)
			{
				//TODO : Need to merge edge compounds once we have the final edge configuration.	
			}

			Context->GraphBuilder->Write(Context);
		}
		else
		{
			Context->CompoundPoints->InitializeOutput(PCGExData::EInit::NoOutput); // Cleanup points
		}

		Context->Done();
	}

	if (Context->IsState(PCGExGraph::State_MergingEdgeCompounds))
	{
		//TODO	
	}

#pragma endregion 
	
	if (Context->IsDone())
	{
		Context->OutputMainPoints();
		Context->PostProcessOutputs();
	}

	return Context->IsDone();
}

namespace PCGExPathToClusters
{
#pragma region NonFusing

	PCGExPathToClusters::FNonFusingProcessor::FNonFusingProcessor(PCGExData::FPointIO* InPoints)
		: FPointsProcessor(InPoints)
	{
	}

	FNonFusingProcessor::~FNonFusingProcessor()
	{
		PCGEX_DELETE(GraphBuilder)
	}

	bool FNonFusingProcessor::Process(FPCGExAsyncManager* AsyncManager)
	{
		const FPCGExPathToEdgeClustersContext* TypedContext = GetContext<FPCGExPathToEdgeClustersContext>();
		PCGEX_SETTINGS(PathToEdgeClusters)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		GraphBuilder = new PCGExGraph::FGraphBuilder(*PointIO, &TypedContext->GraphBuilderSettings, 2);

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		const int32 NumPoints = InPoints.Num();

		PointIO->InitializeOutput(PCGExData::EInit::DuplicateInput);

		TArray<PCGExGraph::FUnsignedEdge> Edges;

		Edges.SetNum(Settings->bClosedPath ? NumPoints : NumPoints - 1);

		for (int i = 0; i < Edges.Num(); i++)
		{
			Edges[i].Start = i;
			Edges[i].End = i + 1;
		}

		if (Settings->bClosedPath)
		{
			Edges.Last().Start = NumPoints - 1;
			Edges.Last().End = 0;
		}

		GraphBuilder->Graph->InsertEdges(Edges, -1);
		Edges.Empty();

		GraphBuilder->Compile(AsyncManager);

		return true;
	}

	void FNonFusingProcessor::CompleteWork()
	{
		GraphBuilder->Write(Context);
	}

#pragma endregion

#pragma region Fusing

	FFusingProcessor::FFusingProcessor(PCGExData::FPointIO* InPoints)
		: FPointsProcessor(InPoints)
	{
	}

	FFusingProcessor::~FFusingProcessor()
	{
	}

	bool FFusingProcessor::Process(FPCGExAsyncManager* AsyncManager)
	{
		const FPCGExPathToEdgeClustersContext* TypedContext = GetContext<FPCGExPathToEdgeClustersContext>();
		PCGEX_SETTINGS(PathToEdgeClusters)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		const int32 NumPoints = InPoints.Num();
		const int32 IOIndex = PointIO->IOIndex;

		if (NumPoints < 2) { return false; }

		for (int i = 0; i < NumPoints; i++)
		{
			PCGExGraph::FCompoundNode* CurrentVtx = CompoundGraph->GetOrCreateNode(InPoints[i], IOIndex, i);

			if (const int32 PrevIndex = i - 1;
				InPoints.IsValidIndex(PrevIndex))
			{
				PCGExGraph::FCompoundNode* OtherVtx = CompoundGraph->GetOrCreateNode(InPoints[PrevIndex], IOIndex, PrevIndex);
				CurrentVtx->Add(OtherVtx);
			}

			if (const int32 NextIndex = i + 1;
				InPoints.IsValidIndex(NextIndex))
			{
				PCGExGraph::FCompoundNode* OtherVtx = CompoundGraph->GetOrCreateNode(InPoints[NextIndex], IOIndex, NextIndex);
				CurrentVtx->Add(OtherVtx);
			}
		}

		if (Settings->bClosedPath)
		{
			// Create an edge between first and last points
			const int32 LastIndex = NumPoints - 1;
			CompoundGraph->CreateBridge(
				InPoints[0], IOIndex, 0,
				InPoints[LastIndex], IOIndex, LastIndex);
		}

		return true;
	}

	FFusingProcessorBatch::FFusingProcessorBatch(FPCGContext* InContext, const TArray<PCGExData::FPointIO*>& InPointsCollection):
		TBatch(InContext, InPointsCollection)
	{
		bInlineProcessing = true; // Points need to be added in the same order to yield deterministic results
	}

	FFusingProcessorBatch::~FFusingProcessorBatch()
	{
		PCGEX_DELETE(CompoundGraph)
		PCGEX_DELETE(CompoundPoints)
		PCGEX_DELETE(CompoundPointsBlender)
	}

	void FFusingProcessorBatch::Process(FPCGExAsyncManager* AsyncManager)
	{
		const FPCGExPathToEdgeClustersContext* TypedContext = GetContext<FPCGExPathToEdgeClustersContext>();
		PCGEX_SETTINGS(PathToEdgeClusters)

		FBox Bounds = FBox(ForceInit);
		for (const PCGExData::FPointIO* IO : PointsCollection) { Bounds += IO->GetIn()->GetBounds(); }

		MainPoints = TypedContext->MainPoints;
		CompoundGraph = new PCGExGraph::FCompoundGraph(Settings->PointPointIntersectionSettings.FuseSettings, Bounds);

		TBatch<FFusingProcessor>::Process(AsyncManager);
	}

	bool FFusingProcessorBatch::PrepareSingle(FFusingProcessor* PointsProcessor)
	{
		const FPCGExPathToEdgeClustersContext* TypedContext = GetContext<FPCGExPathToEdgeClustersContext>();
		PCGEX_SETTINGS(PathToEdgeClusters)

		if (!TBatch<FFusingProcessor>::PrepareSingle(PointsProcessor)) { return false; }

		// TODO:
		// Flow is
		// - Merge all input points to a compound graph
		// - Translate the compound graph node to a "consolidated point" data point
		// - Merge data into those points a first time
		// - Use these points to build a regular graph
		// - Use the regular graph to find & build intersection data
		// - Process and add these intersections
		// - finally compile the graph


		PointsProcessor->GraphBuilder = GraphBuilder;
		PointsProcessor->CompoundGraph = CompoundGraph;

		return true;
	}

	void FFusingProcessorBatch::CompleteWork()
	{
		const FPCGExPathToEdgeClustersContext* TypedContext = GetContext<FPCGExPathToEdgeClustersContext>();
		PCGEX_SETTINGS(PathToEdgeClusters)

		MainPoints = TypedContext->MainPoints;

		CompoundPointsBlender = new PCGExDataBlending::FCompoundBlender(&Settings->DefaultPointsBlendingSettings);
		CompoundPointsBlender->AddSources(*MainPoints);

		CompoundPoints = &MainPoints->Emplace_GetRef(PCGExData::EInit::NewOutput);
		const int32 NumCompoundedNodes = CompoundGraph->NumNodes();
		CompoundPoints->SetNumInitialized(NumCompoundedNodes, true);

		CompoundPointsBlender->PrepareMerge(CompoundPoints, CompoundGraph->PointsCompounds);

		StartParallelLoopForRange(NumCompoundedNodes); // Update point center & blend

		TBatch<FFusingProcessor>::CompleteWork();
	}

	void FFusingProcessorBatch::ProcessSingleRangeIteration(const int32 Iteration)
	{
		// Update center
		CompoundPoints->GetMutablePoint(Iteration).Transform.SetLocation(
			CompoundGraph->Nodes[Iteration]->UpdateCenter(CompoundGraph->PointsCompounds, MainPoints));

		// Process data merging
		CompoundPointsBlender->MergeSingle(Iteration, PCGExSettings::GetDistanceSettings(PointPointIntersectionSettings));
	}

#pragma endregion
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
