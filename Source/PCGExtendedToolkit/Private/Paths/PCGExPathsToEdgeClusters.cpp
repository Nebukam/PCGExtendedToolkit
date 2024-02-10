// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathsToEdgeClusters.h"

#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExPathsToEdgeClustersElement"
#define PCGEX_NAMESPACE BuildCustomGraph

namespace PCGExGraph
{
}

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

PCGExData::EInit UPCGExPathsToEdgeClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FName UPCGExPathsToEdgeClustersSettings::GetMainInputLabel() const { return PCGExGraph::SourcePathsLabel; }

FName UPCGExPathsToEdgeClustersSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(PathsToEdgeClusters)

FPCGExPathsToEdgeClustersContext::~FPCGExPathsToEdgeClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(LooseGraph)
	PCGEX_DELETE(GraphBuilder)
	PCGEX_DELETE(PointEdgeIntersections)
	PCGEX_DELETE(EdgeEdgeIntersections)
}

bool FPCGExPathsToEdgeClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathsToEdgeClusters)

	PCGEX_FWD(FuseSettings)
	PCGEX_FWD(PointEdgeIntersection)
	PCGEX_FWD(EdgeEdgeIntersection)
	
	Context->FuseSettings.FuseSettings.Init();
	Context->EdgeEdgeIntersection.Init();

	Context->GraphMetadataSettings.Grab(Context, Context->FuseSettings);
	Context->GraphMetadataSettings.Grab(Context, Context->PointEdgeIntersection);
	Context->GraphMetadataSettings.Grab(Context, Context->EdgeEdgeIntersection);

	//Context->PointEdgeIntersection.MakeSafeForTolerance(Context->FuseSettings.FuseSettings.Tolerance);
	//Context->EdgeEdgeIntersection.MakeSafeForTolerance(Context->PointEdgeIntersection.FuseSettings.Tolerance);

	PCGEX_FWD(GraphBuilderSettings)

	Context->LooseGraph = new PCGExGraph::FLooseGraph(Context->FuseSettings.FuseSettings);

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
			if (Context->LooseGraph->Nodes.IsEmpty()) { return true; }
			Context->ConsolidatedPoints = &Context->MainPoints->Emplace_GetRef(PCGExData::EInit::NewOutput);
			Context->SetState(PCGExGraph::State_ProcessingGraph);
		}
		else
		{
			Context->GetAsyncManager()->Start<FPCGExInsertPathToLooseGraphTask>(
				Context->CurrentIO->IOIndex, Context->CurrentIO, Context->LooseGraph, Settings->bClosedPath);

			Context->SetAsyncState(PCGExMT::State_ProcessingPoints);
		}
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingGraph))
	{
		const int32 NumLooseNodes = Context->LooseGraph->Nodes.Num();

		auto Initialize = [&]()
		{
			TArray<FPCGPoint>& MutablePoints = Context->ConsolidatedPoints->GetOut()->GetMutablePoints();
			MutablePoints.SetNum(NumLooseNodes);
		};

		auto ProcessNode = [&](int32 Index)
		{
			Context->ConsolidatedPoints->GetMutablePoint(Index).Transform.SetLocation(
				Context->LooseGraph->Nodes[Index]->UpdateCenter(Context->MainPoints));
		};

		if (!Context->Process(Initialize, ProcessNode, NumLooseNodes)) { return false; }

		Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->ConsolidatedPoints, &Context->GraphBuilderSettings, 4);

		TArray<PCGExGraph::FUnsignedEdge> UniqueEdges;
		Context->LooseGraph->GetUniqueEdges(UniqueEdges);
		Context->LooseGraph->WriteMetadata(Context->GraphBuilder->Graph->NodeMetadata);
		PCGEX_DELETE(Context->LooseGraph)

		Context->GraphBuilder->Graph->InsertEdges(UniqueEdges, -1); //TODO : valid IOIndex from LooseGraph
		UniqueEdges.Empty();

		if (Settings->bDoPointEdgeIntersection)
		{
			Context->PointEdgeIntersections = new PCGExGraph::FPointEdgeIntersections(Context->GraphBuilder->Graph, Context->ConsolidatedPoints, Context->PointEdgeIntersection);
			Context->PointEdgeIntersections->FindIntersections(Context);
			Context->SetAsyncState(PCGExGraph::State_FindingPointEdgeIntersections);
		}
		else if (Settings->bDoEdgeEdgeIntersection)
		{
			Context->EdgeEdgeIntersections = new PCGExGraph::FEdgeEdgeIntersections(Context->GraphBuilder->Graph, Context->ConsolidatedPoints, Context->EdgeEdgeIntersection);
			Context->EdgeEdgeIntersections->FindIntersections(Context);
			Context->SetAsyncState(PCGExGraph::State_FindingEdgeEdgeIntersections);
		}
		else
		{
			Context->SetAsyncState(PCGExGraph::State_WritingClusters);
		}
	}

	if (Context->IsState(PCGExGraph::State_FindingPointEdgeIntersections))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		Context->PointEdgeIntersections->Insert(); // TODO : Async?
		PCGEX_DELETE(Context->PointEdgeIntersections)

		if (Settings->bDoEdgeEdgeIntersection)
		{
			Context->EdgeEdgeIntersections = new PCGExGraph::FEdgeEdgeIntersections(Context->GraphBuilder->Graph, Context->ConsolidatedPoints, Context->EdgeEdgeIntersection);
			Context->EdgeEdgeIntersections->FindIntersections(Context);
			Context->SetAsyncState(PCGExGraph::State_FindingEdgeEdgeIntersections);
		}
		else
		{
			Context->SetAsyncState(PCGExGraph::State_WritingClusters);
		}
	}

	if (Context->IsState(PCGExGraph::State_FindingEdgeEdgeIntersections))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		Context->EdgeEdgeIntersections->Insert(); // TODO : Async?
		PCGEX_DELETE(Context->EdgeEdgeIntersections)

		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		Context->GraphBuilder->Compile(Context, &Context->GraphMetadataSettings);
		Context->SetAsyncState(PCGExGraph::State_WaitingOnWritingClusters);
		return false;
	}

	if (Context->IsState(PCGExGraph::State_WaitingOnWritingClusters))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		if (Context->GraphBuilder->bCompiledSuccessfully)
		{
			Context->GraphBuilder->Write(Context);
			Context->OutputPoints();
		}

		Context->Done();
	}

	return Context->IsDone();
}

bool FPCGExInsertPathToLooseGraphTask::ExecuteTask()
{
	const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
	const int32 NumPoints = InPoints.Num();

	if (NumPoints < 2) { return false; }

	for (int i = 0; i < NumPoints; i++)
	{
		PCGExGraph::FLooseNode* CurrentVtx = Graph->GetOrCreateNode(InPoints[i], TaskIndex, i);
		CurrentVtx->AddPointH(PCGEx::H64(TaskIndex, i));

		if (const int32 PrevIndex = i - 1;
			InPoints.IsValidIndex(PrevIndex))
		{
			PCGExGraph::FLooseNode* OtherVtx = Graph->GetOrCreateNode(InPoints[PrevIndex], TaskIndex, PrevIndex);
			CurrentVtx->Add(OtherVtx);
		}

		if (const int32 NextIndex = i + 1;
			InPoints.IsValidIndex(NextIndex))
		{
			PCGExGraph::FLooseNode* OtherVtx = Graph->GetOrCreateNode(InPoints[NextIndex], TaskIndex, NextIndex);
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
