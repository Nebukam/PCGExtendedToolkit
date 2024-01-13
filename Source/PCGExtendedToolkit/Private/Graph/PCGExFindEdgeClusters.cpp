// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFindEdgeClusters.h"

#include "Data/PCGExData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE FindEdgeClusters

int32 UPCGExFindEdgeClustersSettings::GetPreferredChunkSize() const { return 32; }

PCGExData::EInit UPCGExFindEdgeClustersSettings::GetMainOutputInitMode() const { return bPruneIsolatedPoints ? PCGExData::EInit::NewOutput : PCGExData::EInit::DuplicateInput; }

FPCGExFindEdgeClustersContext::~FPCGExFindEdgeClustersContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_DELETE(NetworkBuilder)
}

TArray<FPCGPinProperties> UPCGExFindEdgeClustersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PinProperties.Pop(); //Remove graph output
	FPCGPinProperties& PinClustersOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinClustersOutput.Tooltip = FTEXT("Point data representing edges.");
#endif


	return PinProperties;
}

FName UPCGExFindEdgeClustersSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(FindEdgeClusters)

bool FPCGExFindEdgeClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExCustomGraphProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindEdgeClusters)

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

bool FPCGExFindEdgeClustersElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindEdgeClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FindEdgeClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->NetworkBuilder)

		if (!Context->AdvancePointsIOAndResetGraph()) { Context->Done(); }
		else
		{
			Context->NetworkBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, Context->MergedInputSocketsNum);
			if (Settings->bFindCrossings) { Context->NetworkBuilder->EnableCrossings(Settings->CrossingTolerance); }
			if (Settings->bPruneIsolatedPoints) { Context->NetworkBuilder->EnablePointsPruning(); }

			Context->SetState(PCGExGraph::State_ReadyForNextGraph);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph())
		{
			if (Context->NetworkBuilder->EdgeCrossings) { Context->SetState(PCGExGraph::State_FindingCrossings); }
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

			for (const PCGExGraph::FSocketInfos& SocketInfo : Context->SocketInfos)
			{
				const int32 End = SocketInfo.Socket->GetTargetIndexReader().Values[PointIndex];
				const int32 InEdgeType = SocketInfo.Socket->GetEdgeTypeReader().Values[PointIndex];
				if (End != -1 && (InEdgeType & EdgeType) != 0) { Context->NetworkBuilder->Graph->InsertEdge(PointIndex, End); }
			}
		};

		if (!Context->ProcessCurrentPoints(Initialize, InsertEdge)) { return false; }
		Context->SetState(PCGExGraph::State_ReadyForNextGraph);
	}

	if (Context->IsState(PCGExGraph::State_FindingCrossings))
	{
		auto Initialize = [&]()
		{
			Context->NetworkBuilder->EdgeCrossings->Prepare(Context->CurrentIO->GetIn()->GetPoints());
		};

		auto ProcessEdge = [&](const int32 Index)
		{
			Context->NetworkBuilder->EdgeCrossings->ProcessEdge(Index, Context->CurrentIO->GetIn()->GetPoints());
		};

		if (!Context->Process(Initialize, ProcessEdge, Context->NetworkBuilder->Graph->Edges.Num())) { return false; }
		Context->SetState(PCGExGraph::State_WritingClusters);
	}

	// -> Network is ready

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		if (Context->NetworkBuilder->Compile(Context, Context->MinClusterSize, Context->MaxClusterSize))
		{
			Context->SetAsyncState(PCGExGraph::State_WaitingOnWritingClusters);
		}
		else
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	if (Context->IsState(PCGExGraph::State_WaitingOnWritingClusters))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		Context->NetworkBuilder->Write(Context);
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
