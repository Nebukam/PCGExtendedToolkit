// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathsToEdgeClusters.h"
#include "Graph/PCGExGraph.h"
#include "Data/Blending/PCGExCompoundBlender.h"

#define LOCTEXT_NAMESPACE "PCGExPathsToEdgeClustersElement"
#define PCGEX_NAMESPACE BuildCustomGraph

UPCGExPathsToEdgeClustersSettings::UPCGExPathsToEdgeClustersSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPCGExPathsToEdgeClustersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinClustersOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinClustersOutput.Tooltip = FTEXT("Point data representing edges.");
#endif


	return PinProperties;
}

#if WITH_EDITOR
void UPCGExPathsToEdgeClustersSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGExData::EInit UPCGExPathsToEdgeClustersSettings::GetMainOutputInitMode() const { return bFusePaths ? PCGExData::EInit::NoOutput : PCGExData::EInit::DuplicateInput; }

FName UPCGExPathsToEdgeClustersSettings::GetMainInputLabel() const { return PCGExGraph::SourcePathsLabel; }

FName UPCGExPathsToEdgeClustersSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(PathsToEdgeClusters)

FPCGExPathsToEdgeClustersContext::~FPCGExPathsToEdgeClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(CompoundGraph)
	PCGEX_DELETE(GraphBuilder)
	PCGEX_DELETE(PointEdgeIntersections)
	PCGEX_DELETE(EdgeEdgeIntersections)

	PCGEX_DELETE(CompoundPointsBlender)
}

bool FPCGExPathsToEdgeClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathsToEdgeClusters)

	PCGEX_FWD(PointPointIntersectionSettings)
	PCGEX_FWD(PointEdgeIntersectionSettings)
	PCGEX_FWD(EdgeEdgeIntersectionSettings)

	Context->EdgeEdgeIntersectionSettings.ComputeDot();

	Context->GraphMetadataSettings.Grab(Context, Context->PointPointIntersectionSettings);
	Context->GraphMetadataSettings.Grab(Context, Context->PointEdgeIntersectionSettings);
	Context->GraphMetadataSettings.Grab(Context, Context->EdgeEdgeIntersectionSettings);

	Context->CompoundPointsBlender = new PCGExDataBlending::FCompoundBlender(const_cast<FPCGExBlendingSettings*>(&Settings->PointsBlendingSettings));
	Context->CompoundPointsBlender->AddSources(*Context->MainPoints);

	PCGEX_FWD(GraphBuilderSettings)

	Context->CompoundGraph = new PCGExGraph::FCompoundGraph(Context->PointPointIntersectionSettings.FuseSettings, Context->MainPoints->GetInBounds().ExpandBy(10));

	return true;
}


bool FPCGExPathsToEdgeClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathsToEdgeClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathsToEdgeClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			if (Settings->bFusePaths)
			{
				if (Context->CompoundGraph->Nodes.IsEmpty()) { return true; }
				Context->ConsolidatedPoints = &Context->MainPoints->Emplace_GetRef(PCGExData::EInit::NewOutput);
				Context->SetState(PCGExGraph::State_ProcessingGraph);
			}
			else
			{
				PCGEX_DELETE(Context->GraphBuilder)
				Context->Done();
			}
		}
		else
		{
			if (Settings->bFusePaths)
			{
				Context->GetAsyncManager()->Start<FPCGExInsertPathToCompoundGraphTask>(
					Context->CurrentIO->IOIndex, Context->CurrentIO, Context->CompoundGraph, Settings->bClosedPath);

				Context->SetAsyncState(PCGExMT::State_ProcessingPoints);
			}
			else
			{
				PCGEX_DELETE(Context->GraphBuilder)

				//TODO: Create one graph per path
				const TArray<FPCGPoint>& InPoints = Context->CurrentIO->GetIn()->GetPoints();
				const int32 NumPoints = InPoints.Num();

				if (NumPoints < 2) { return false; }

				Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, &Context->GraphBuilderSettings, 2);
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

				Context->GraphBuilder->Graph->InsertEdges(Edges, -1);
				Edges.Empty();

				Context->SetState(PCGExGraph::State_WritingClusters);
			}
		}
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		PCGEX_WAIT_ASYNC
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingGraph))
	{
		// Create consolidated set of points from

		const int32 NumCompoundNodes = Context->CompoundGraph->Nodes.Num();
		TArray<FPCGPoint>& MutablePoints = Context->ConsolidatedPoints->GetOut()->GetMutablePoints();

		auto Initialize = [&]() { Context->ConsolidatedPoints->SetNumInitialized(NumCompoundNodes, true); };

		auto ProcessNode = [&](const int32 Index)
		{
			Context->ConsolidatedPoints->GetMutablePoint(Index).Transform.SetLocation(
				Context->CompoundGraph->Nodes[Index]->UpdateCenter(Context->CompoundGraph->PointsCompounds, Context->MainPoints));
			Context->ConsolidatedPoints->GetOut()->Metadata->InitializeOnSet(MutablePoints[Index].MetadataEntry);
		};

		if (!Context->Process(Initialize, ProcessNode, NumCompoundNodes)) { return false; }

		// Initiate merging
		Context->CompoundPointsBlender->PrepareMerge(Context->ConsolidatedPoints, Context->CompoundGraph->PointsCompounds);
		Context->SetState(PCGExData::State_MergingData);
	}

	if (Context->IsState(PCGExData::State_MergingData))
	{
		auto MergeCompound = [&](const int32 CompoundIndex)
		{
			Context->CompoundPointsBlender->MergeSingle(CompoundIndex, PCGExSettings::GetDistanceSettings(Context->PointPointIntersectionSettings));
		};

		if (!Context->Process(MergeCompound, Context->CompoundGraph->NumNodes())) { return false; }

		Context->CompoundPointsBlender->Write();

		// Build final graph from compound graph

		Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->ConsolidatedPoints, &Context->GraphBuilderSettings, 4);

		TArray<PCGExGraph::FUnsignedEdge> UniqueEdges;
		Context->CompoundGraph->GetUniqueEdges(UniqueEdges);
		Context->CompoundGraph->WriteMetadata(Context->GraphBuilder->Graph->NodeMetadata);

		Context->GraphBuilder->Graph->InsertEdges(UniqueEdges, -1); //TODO : valid IOIndex from CompoundGraph
		UniqueEdges.Empty();

		if (Settings->bDoPointEdgeIntersection)
		{
			Context->PointEdgeIntersections = new PCGExGraph::FPointEdgeIntersections(Context->GraphBuilder->Graph, Context->CompoundGraph, Context->ConsolidatedPoints, Context->PointEdgeIntersectionSettings);
			Context->SetState(PCGExGraph::State_FindingPointEdgeIntersections);
		}
		else if (Settings->bDoEdgeEdgeIntersection)
		{
			Context->EdgeEdgeIntersections = new PCGExGraph::FEdgeEdgeIntersections(Context->GraphBuilder->Graph, Context->CompoundGraph, Context->ConsolidatedPoints, Context->EdgeEdgeIntersectionSettings);
			Context->SetState(PCGExGraph::State_FindingEdgeEdgeIntersections);
		}
		else
		{
			Context->SetAsyncState(PCGExGraph::State_WritingClusters);
		}
	}

	if (Context->IsState(PCGExGraph::State_FindingPointEdgeIntersections))
	{
		auto PointEdge = [&](const int32 EdgeIndex)
		{
			const PCGExGraph::FIndexedEdge& Edge = Context->GraphBuilder->Graph->Edges[EdgeIndex];
			if (!Edge.bValid) { return; }
			FindCollinearNodes(Context->PointEdgeIntersections, EdgeIndex, Context->ConsolidatedPoints->GetOut());
		};

		if (!Context->Process(PointEdge, Context->GraphBuilder->Graph->Edges.Num())) { return false; }

		Context->PointEdgeIntersections->Insert(); // TODO : Async?
		PCGEX_DELETE(Context->PointEdgeIntersections)

		if (Settings->bDoEdgeEdgeIntersection)
		{
			Context->EdgeEdgeIntersections = new PCGExGraph::FEdgeEdgeIntersections(Context->GraphBuilder->Graph, Context->CompoundGraph, Context->ConsolidatedPoints, Context->EdgeEdgeIntersectionSettings);
			Context->SetState(PCGExGraph::State_FindingEdgeEdgeIntersections);
		}
		else
		{
			Context->SetAsyncState(PCGExGraph::State_WritingClusters);
		}
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
		PCGEX_DELETE(Context->EdgeEdgeIntersections)

		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		PCGEX_WAIT_ASYNC

		Context->GraphBuilder->Compile(Context, &Context->GraphMetadataSettings);
		Context->SetAsyncState(PCGExGraph::State_WaitingOnWritingClusters);
		return false;
	}

	if (Context->IsState(PCGExGraph::State_WaitingOnWritingClusters))
	{
		PCGEX_WAIT_ASYNC

		if (Context->GraphBuilder->bCompiledSuccessfully) { Context->GraphBuilder->Write(Context); }

		if (Settings->bFusePaths) { Context->Done(); }
		else { Context->SetState(PCGExMT::State_ReadyForNextPoints); }
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
	}

	return Context->IsDone();
}

bool FPCGExInsertPathToCompoundGraphTask::ExecuteTask()
{
	const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
	const int32 NumPoints = InPoints.Num();

	if (NumPoints < 2) { return false; }

	for (int i = 0; i < NumPoints; i++)
	{
		PCGExGraph::FCompoundNode* CurrentVtx = Graph->GetOrCreateNode(InPoints[i], TaskIndex, i);

		if (const int32 PrevIndex = i - 1;
			InPoints.IsValidIndex(PrevIndex))
		{
			PCGExGraph::FCompoundNode* OtherVtx = Graph->GetOrCreateNode(InPoints[PrevIndex], TaskIndex, PrevIndex);
			CurrentVtx->Add(OtherVtx);
		}

		if (const int32 NextIndex = i + 1;
			InPoints.IsValidIndex(NextIndex))
		{
			PCGExGraph::FCompoundNode* OtherVtx = Graph->GetOrCreateNode(InPoints[NextIndex], TaskIndex, NextIndex);
			CurrentVtx->Add(OtherVtx);
		}
	}

	if (bJoinFirstAndLast)
	{
		// Join
		const int32 LastIndex = NumPoints - 1;
		Graph->CreateBridge(
			InPoints[0], TaskIndex, 0,
			InPoints[LastIndex], TaskIndex, LastIndex);
	}

	return true;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
