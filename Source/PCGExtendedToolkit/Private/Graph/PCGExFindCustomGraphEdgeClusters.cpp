// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFindCustomGraphEdgeClusters.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE FindCustomGraphEdgeClusters

int32 UPCGExFindCustomGraphEdgeClustersSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_M; }

PCGExData::EInit UPCGExFindCustomGraphEdgeClustersSettings::GetMainOutputInitMode() const { return GraphBuilderSettings.bPruneIsolatedPoints ? PCGExData::EInit::NewOutput : PCGExData::EInit::DuplicateInput; }

FPCGExFindCustomGraphEdgeClustersContext::~FPCGExFindCustomGraphEdgeClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	UniqueEdges.Empty();
	Edges.Empty();

	PCGEX_DELETE(GraphBuilder)
}

TArray<FPCGPinProperties> UPCGExFindCustomGraphEdgeClustersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PinProperties.Pop(); //Remove graph output
	FPCGPinProperties& PinClustersOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinClustersOutput.Tooltip = FTEXT("Point data representing edges.");
#endif


	return PinProperties;
}

FName UPCGExFindCustomGraphEdgeClustersSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(FindCustomGraphEdgeClusters)

bool FPCGExFindCustomGraphEdgeClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExCustomGraphProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindCustomGraphEdgeClusters)

	Context->EdgeCrawlingSettings = Settings->EdgeCrawlingSettings;

	PCGEX_FWD(GraphBuilderSettings)
	Context->GraphBuilderSettings.bRefreshEdgeSeed = true;

	return true;
}

bool FPCGExFindCustomGraphEdgeClustersElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindCustomGraphEdgeClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FindCustomGraphEdgeClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->GraphBuilder)

		if (!Context->AdvancePointsIOAndResetGraph()) { Context->Done(); }
		else
		{
			Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, &Context->GraphBuilderSettings, Context->MergedInputSocketsNum);
			Context->SetState(PCGExGraph::State_ReadyForNextGraph);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph()) { Context->SetState(PCGExGraph::State_WritingClusters); }
		else
		{
			Context->CurrentIO->CreateInKeys();

			if (!Context->PrepareCurrentGraphForPoints(*Context->CurrentIO))
			{
				PCGEX_GRAPH_MISSING_METADATA
				return false;
			}

			Context->SetState(PCGExGraph::State_BuildCustomGraph);
		}
	}

	// -> Process current points with current graph

	if (Context->IsState(PCGExGraph::State_BuildCustomGraph))
	{
		auto InsertEdge = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			const int32 EdgeType = Context->CurrentGraphEdgeCrawlingTypes;
			for (const PCGExGraph::FSocketInfos& SocketInfo : Context->SocketInfos)
			{
				const int32 End = SocketInfo.Socket->GetTargetIndexReader().Values[PointIndex];

				if (End == -1 || PointIndex == End) { continue; }

				const uint64 Hash = PCGEx::H64U(PointIndex, End);

				const int32 InEdgeType = SocketInfo.Socket->GetEdgeTypeReader().Values[PointIndex];
				if ((InEdgeType & EdgeType) == 0) { continue; }

				{
					FReadScopeLock ReadLock(Context->UniqueEdgesLock);
					if (Context->UniqueEdges.Contains(Hash)) { continue; }
				}
				{
					FWriteScopeLock WriteLock(Context->UniqueEdgesLock);
					Context->UniqueEdges.Add(Hash);
					Context->Edges.Emplace(PointIndex, End);
				}
			}
		};

		if (!Context->ProcessCurrentPoints(InsertEdge)) { return false; }

		Context->GraphBuilder->Graph->InsertEdges(Context->Edges, -1);

		Context->UniqueEdges.Reset();
		Context->Edges.Reset();

		Context->SetState(PCGExGraph::State_ReadyForNextGraph);
	}

	// -> Network is ready

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		Context->GraphBuilder->Compile(Context);
		Context->SetAsyncState(PCGExGraph::State_WaitingOnWritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WaitingOnWritingClusters))
	{
		PCGEX_WAIT_ASYNC
		if (Context->GraphBuilder->bCompiledSuccessfully) { Context->GraphBuilder->Write(Context); }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		if (Settings->bDeleteCustomGraphData)
		{
			Context->MainPoints->ForEach(
				[&](const PCGExData::FPointIO& PointIO, int32)
				{
					auto DeleteSockets = [&](const UPCGExGraphDefinition* Params, int32)
					{
						const UPCGPointData* OutData = PointIO.GetOut();
						for (const PCGExGraph::FSocket& Socket : Params->GetSocketMapping()->Sockets)
						{
							Socket.DeleteFrom(OutData);
						}
						OutData->Metadata->DeleteAttribute(Params->CachedIndexAttributeName);
					};
					Context->Graphs.ForEach(Context, DeleteSockets);
				});
		}
		Context->OutputPoints();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
