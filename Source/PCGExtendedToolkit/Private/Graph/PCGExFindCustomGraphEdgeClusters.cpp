// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFindCustomGraphEdgeClusters.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE FindCustomGraphEdgeClusters

int32 UPCGExFindCustomGraphEdgeClustersSettings::GetPreferredChunkSize() const { return 32; }

PCGExData::EInit UPCGExFindCustomGraphEdgeClustersSettings::GetMainOutputInitMode() const { return bPruneIsolatedPoints ? PCGExData::EInit::NewOutput : PCGExData::EInit::DuplicateInput; }

FPCGExFindCustomGraphEdgeClustersContext::~FPCGExFindCustomGraphEdgeClustersContext()
{
	PCGEX_TERMINATE_ASYNC
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

	Context->CrawlEdgeTypes = static_cast<EPCGExEdgeType>(Settings->CrawlEdgeTypes);

	PCGEX_FWD(bInheritAttributes)

	Context->MinClusterSize = Settings->bRemoveSmallClusters ? FMath::Max(1, Settings->MinClusterSize) : 1;
	Context->MaxClusterSize = Settings->bRemoveBigClusters ? FMath::Max(1, Settings->MaxClusterSize) : TNumericLimits<int32>::Max();

	PCGEX_FWD(ClusterIDAttributeName)
	PCGEX_FWD(ClusterSizeAttributeName)

	PCGEX_VALIDATE_NAME(Context->ClusterIDAttributeName)
	PCGEX_VALIDATE_NAME(Context->ClusterSizeAttributeName)

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
			Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, Context->MergedInputSocketsNum);
			if (Settings->bFindCrossings) { Context->GraphBuilder->EnableCrossings(Settings->CrossingTolerance); }
			if (Settings->bPruneIsolatedPoints) { Context->GraphBuilder->EnablePointsPruning(); }

			Context->SetState(PCGExGraph::State_ReadyForNextGraph);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph())
		{
			if (Context->GraphBuilder->EdgeCrossings) { Context->SetState(PCGExGraph::State_FindingCrossings); }
			else { Context->SetState(PCGExGraph::State_WritingClusters); }
		}
		else { Context->SetState(PCGExGraph::State_BuildCustomGraph); }
	}

	// -> Process current points with current graph

	if (Context->IsState(PCGExGraph::State_BuildCustomGraph))
	{
		auto Initialize = [&](PCGExData::FPointIO& PointIO)
		{
			PointIO.CreateInKeys();
			Context->PrepareCurrentGraphForPoints(PointIO);
		};

		auto InsertEdge = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			const int32 EdgeType = static_cast<int32>(Context->CrawlEdgeTypes);
			PCGExGraph::FIndexedEdge NewEdge = PCGExGraph::FIndexedEdge{};

			for (const PCGExGraph::FSocketInfos& SocketInfo : Context->SocketInfos)
			{
				const int32 End = SocketInfo.Socket->GetTargetIndexReader().Values[PointIndex];
				const int32 InEdgeType = SocketInfo.Socket->GetEdgeTypeReader().Values[PointIndex];
				if (End != -1 && (InEdgeType & EdgeType) != 0) { Context->GraphBuilder->Graph->InsertEdge(PointIndex, End, NewEdge); }
			}
		};

		if (!Context->ProcessCurrentPoints(Initialize, InsertEdge)) { return false; }
		Context->SetState(PCGExGraph::State_ReadyForNextGraph);
	}

	if (Context->IsState(PCGExGraph::State_FindingCrossings))
	{
		auto Initialize = [&]()
		{
			Context->GraphBuilder->EdgeCrossings->Prepare(Context->CurrentIO->GetIn()->GetPoints());
		};

		auto ProcessEdge = [&](const int32 Index)
		{
			Context->GraphBuilder->EdgeCrossings->ProcessEdge(Index, Context->CurrentIO->GetIn()->GetPoints());
		};

		if (!Context->Process(Initialize, ProcessEdge, Context->GraphBuilder->Graph->Edges.Num())) { return false; }
		Context->SetState(PCGExGraph::State_WritingClusters);
	}

	// -> Network is ready

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		Context->GraphBuilder->Compile(Context, Context->MinClusterSize, Context->MaxClusterSize);
		Context->SetAsyncState(PCGExGraph::State_WaitingOnWritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WaitingOnWritingClusters))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
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
					auto DeleteSockets = [&](const UPCGExGraphParamsData* Params, int32)
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
