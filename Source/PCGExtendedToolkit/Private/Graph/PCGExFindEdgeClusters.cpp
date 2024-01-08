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

	PCGEX_DELETE(ClustersIO)
	PCGEX_DELETE(EdgeNetwork)
	PCGEX_DELETE(Markings)

	IndexRemap.Empty();
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
	if (!FPCGExGraphProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindEdgeClusters)

	Context->CrawlEdgeTypes = static_cast<EPCGExEdgeType>(Settings->CrawlEdgeTypes);

	PCGEX_FWD(bPruneIsolatedPoints)
	PCGEX_FWD(bInheritAttributes)

	Context->MinClusterSize = Settings->bRemoveSmallClusters ? FMath::Max(1, Settings->MinClusterSize) : 1;
	Context->MaxClusterSize = Settings->bRemoveBigClusters ? FMath::Max(1, Settings->MaxClusterSize) : TNumericLimits<int32>::Max();

	PCGEX_FWD(ClusterIDAttributeName)
	PCGEX_FWD(ClusterSizeAttributeName)

	PCGEX_FWD(bFindCrossings)
	PCGEX_FWD(CrossingTolerance)

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
		PCGEX_DELETE(Context->EdgeNetwork)
		PCGEX_DELETE(Context->ClustersIO)
		PCGEX_DELETE(Context->Markings)
		PCGEX_DELETE(Context->EdgeCrossings)

		if (!Context->AdvancePointsIOAndResetGraph()) { Context->Done(); }
		else
		{
			Context->ClustersIO = new PCGExData::FPointIOGroup();
			Context->ClustersIO->DefaultOutputLabel = PCGExGraph::OutputEdgesLabel;
			Context->EdgeNetwork = new PCGExGraph::FEdgeNetwork(Context->MergedInputSocketsNum, Context->CurrentIO->GetNum());
			Context->Markings = new PCGExData::FKPointIOMarkedBindings<int32>(Context->CurrentIO, PCGExGraph::PUIDAttributeName);
			Context->SetState(PCGExGraph::State_ReadyForNextGraph);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph())
		{
			if (Context->bFindCrossings)
			{
				Context->EdgeCrossings = new PCGExGraph::FEdgeCrossingsHandler(Context->EdgeNetwork, Context->CrossingTolerance);
				Context->SetState(PCGExGraph::State_FindingCrossings);
			}
			else
			{
				Context->SetState(PCGExGraph::State_WritingClusters);
			}
		}
		else { Context->SetState(PCGExGraph::State_BuildNetwork); }
	}

	// -> Process current points with current graph

	if (Context->IsState(PCGExGraph::State_BuildNetwork))
	{
		Context->CurrentIO->CreateInKeys();
		Context->PrepareCurrentGraphForPoints(*Context->CurrentIO);

		const int32 NumNodes = Context->EdgeNetwork->Nodes.Num();
		TSet<int32> VisitedNodes;
		VisitedNodes.Reserve(NumNodes);

		const int32 EdgeType = static_cast<int32>(Context->CrawlEdgeTypes);
		for (int i = 0; i < NumNodes; i++)
		{
			TQueue<int32> Queue;
			Queue.Enqueue(i);

			int32 Index;
			while (Queue.Dequeue(Index))
			{
				if (!VisitedNodes.Contains(Index))
				{
					VisitedNodes.Add(Index);

					for (const PCGExGraph::FSocketInfos& SocketInfo : Context->SocketInfos)
					{
						const int32 End = SocketInfo.Socket->GetTargetIndexReader().Values[Index];
						const int32 InEdgeType = SocketInfo.Socket->GetEdgeTypeReader().Values[Index];

						if (End != -1 && (InEdgeType & EdgeType) != 0)
						{
							Context->EdgeNetwork->InsertEdge(PCGExGraph::FUnsignedEdge(Index, End));
							Queue.Enqueue(End);
						}
					}
				}
			}
		}

		VisitedNodes.Empty();
		Context->SetState(PCGExGraph::State_ReadyForNextGraph);
	}

	if (Context->IsState(PCGExGraph::State_FindingCrossings))
	{
		auto Initialize = [&]()
		{
			Context->EdgeCrossings->Prepare(Context->CurrentIO->GetIn()->GetPoints());
		};

		auto ProcessEdge = [&](const int32 Index)
		{
			Context->EdgeCrossings->ProcessEdge(Index, Context->CurrentIO->GetIn()->GetPoints());
		};

		if (Context->Process(Initialize, ProcessEdge, Context->EdgeNetwork->Edges.Num()))
		{
			Context->EdgeCrossings->InsertCrossings();
			Context->SetState(PCGExGraph::State_WritingClusters);
		}
	}

	// -> Network is ready

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		Context->ClustersIO->Flush();
		Context->EdgeNetwork->PrepareClusters(Context->MinClusterSize, Context->MaxClusterSize);
		Context->Markings->Mark = Context->CurrentIO->GetIn()->GetUniqueID();

		if (Context->bPruneIsolatedPoints)
		{
			UPCGPointData* OutData = Context->CurrentIO->GetOut();
			TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
			const int32 NumMaxNodes = Context->EdgeNetwork->Nodes.Num();
			MutablePoints.Reserve(NumMaxNodes);
			Context->IndexRemap.Empty();
			Context->IndexRemap.Reserve(NumMaxNodes);
			int32 Index = 0;

			for (PCGExGraph::FNetworkNode Node : Context->EdgeNetwork->Nodes)
			{
				if (Node.bCrossing) { continue; }
				if (Node.Cluster == -1 || Node.Edges.IsEmpty()) { continue; }
				if (*Context->EdgeNetwork->ClusterSizes.Find(Node.Cluster) == -1) { continue; }

				Context->IndexRemap.Add(Node.Index, Index++);
				MutablePoints.Add(Context->CurrentIO->GetInPoint(Node.Index));
			}

			if (Context->bFindCrossings)
			{
				const int32 Offset = Context->EdgeCrossings->StartIndex;
				for (int i = 0; i < Context->EdgeCrossings->Crossings.Num(); i++)
				{
					const PCGExGraph::FEdgeCrossing& Crossing = Context->EdgeCrossings->Crossings[i];
					const PCGExGraph::FNetworkNode& Node = Context->EdgeNetwork->Nodes[Offset + i];

					if (Node.Cluster == -1 || Node.Edges.IsEmpty()) { continue; }
					if (*Context->EdgeNetwork->ClusterSizes.Find(Node.Cluster) == -1) { continue; }

					Context->IndexRemap.Add(Offset + i, Index++);
					MutablePoints.Emplace_GetRef().Transform.SetLocation(Crossing.Center);
				}
			}
		}
		else if (Context->bFindCrossings)
		{
			TArray<FPCGPoint>& MutablePoints = Context->CurrentIO->GetOut()->GetMutablePoints();
			for (const PCGExGraph::FEdgeCrossing& Crossing : Context->EdgeCrossings->Crossings)
			{
				MutablePoints.Emplace_GetRef().Transform.SetLocation(Crossing.Center);
			}
		}


		for (const TPair<int32, int32>& Pair : Context->EdgeNetwork->ClusterSizes)
		{
			const int32 ClusterSize = Pair.Value;
			if (ClusterSize == -1) { continue; }

			PCGExData::FPointIO& ClusterIO = Context->ClustersIO->Emplace_GetRef(PCGExData::EInit::NewOutput);
			Context->Markings->Add(ClusterIO);

			Context->GetAsyncManager()->Start<FWriteClusterTask>(
				Pair.Key, Context->CurrentIO, &ClusterIO,
				Context->EdgeNetwork, Context->bPruneIsolatedPoints ? &Context->IndexRemap : nullptr);
		}

		if (Context->ClustersIO->IsEmpty())
		{
			Context->CurrentIO->GetOut()->Metadata->DeleteAttribute(PCGExGraph::PUIDAttributeName); // Unmark
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
		else
		{
			Context->SetAsyncState(PCGExGraph::State_WaitingOnWritingClusters);
		}
	}

	if (Context->IsState(PCGExGraph::State_WaitingOnWritingClusters))
	{
		if (Context->IsAsyncWorkComplete())
		{
			Context->Markings->UpdateMark();
			Context->ClustersIO->OutputTo(Context, true);
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	if (Context->IsDone())
	{
		if (Settings->bDeleteGraphData)
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
