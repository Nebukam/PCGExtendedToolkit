// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFindEdgeIslands.h"

#include "Data/PCGExData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE FindEdgeIslands

int32 UPCGExFindEdgeIslandsSettings::GetPreferredChunkSize() const { return 32; }

PCGExData::EInit UPCGExFindEdgeIslandsSettings::GetMainOutputInitMode() const { return bPruneIsolatedPoints ? PCGExData::EInit::NewOutput : PCGExData::EInit::DuplicateInput; }

FPCGExFindEdgeIslandsContext::~FPCGExFindEdgeIslandsContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(IslandsIO)
	PCGEX_DELETE(Network)
	PCGEX_DELETE(Markings)

	IndexRemap.Empty();
}

TArray<FPCGPinProperties> UPCGExFindEdgeIslandsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PinProperties.Pop(); //Remove graph output
	FPCGPinProperties& PinIslandsOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinIslandsOutput.Tooltip = FTEXT("Point data representing edges.");
#endif // WITH_EDITOR

	//PCGEx::Swap(PinProperties, PinProperties.Num() - 1, PinProperties.Num() - 2);
	return PinProperties;
}

FName UPCGExFindEdgeIslandsSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

FPCGElementPtr UPCGExFindEdgeIslandsSettings::CreateElement() const
{
	return MakeShared<FPCGExFindEdgeIslandsElement>();
}

PCGEX_INITIALIZE_CONTEXT(FindEdgeIslands)

bool FPCGExFindEdgeIslandsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExGraphProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindEdgeIslands)

	Context->CrawlEdgeTypes = static_cast<EPCGExEdgeType>(Settings->CrawlEdgeTypes);

	PCGEX_FWD(bPruneIsolatedPoints)
	PCGEX_FWD(bInheritAttributes)

	Context->MinIslandSize = Settings->bRemoveSmallIslands ? FMath::Max(1, Settings->MinIslandSize) : 1;
	Context->MaxIslandSize = Settings->bRemoveBigIslands ? FMath::Max(1, Settings->MaxIslandSize) : TNumericLimits<int32>::Max();

	PCGEX_FWD(IslandIDAttributeName)
	PCGEX_FWD(IslandSizeAttributeName)

	PCGEX_VALIDATE_NAME(Context->IslandIDAttributeName)
	PCGEX_VALIDATE_NAME(Context->IslandSizeAttributeName)

	return true;
}

bool FPCGExFindEdgeIslandsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindEdgeIslandsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FindEdgeIslands)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->Network)
		PCGEX_DELETE(Context->IslandsIO)
		PCGEX_DELETE(Context->Markings)

		if (!Context->AdvancePointsIOAndResetGraph()) { Context->Done(); }
		else
		{
			Context->IslandsIO = new PCGExData::FPointIOGroup();
			Context->IslandsIO->DefaultOutputLabel = PCGExGraph::OutputEdgesLabel;
			Context->Network = new PCGExGraph::FNetwork(Context->MergedInputSocketsNum, Context->CurrentIO->GetNum());
			Context->Markings = new PCGExData::FKPointIOMarkedBindings<int32>(Context->CurrentIO, PCGExGraph::PUIDAttributeName);
			Context->SetState(PCGExGraph::State_ReadyForNextGraph);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph()) { Context->SetState(PCGExGraph::State_WritingIslands); }
		else { Context->SetState(PCGExGraph::State_BuildNetwork); }
	}

	// -> Process current points with current graph

	if (Context->IsState(PCGExGraph::State_BuildNetwork))
	{
		Context->CurrentIO->CreateInKeys();
		Context->PrepareCurrentGraphForPoints(*Context->CurrentIO);

		const int32 NumNodes = Context->Network->Nodes.Num();
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
							Context->Network->InsertEdge(PCGExGraph::FUnsignedEdge(Index, End, EPCGExEdgeType::Complete));
							Queue.Enqueue(End);
						}
					}
				}
			}
		}

		VisitedNodes.Empty();
		Context->SetState(PCGExGraph::State_ReadyForNextGraph);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (Context->IsAsyncWorkComplete()) { Context->SetState(PCGExGraph::State_ReadyForNextGraph); }
	}

	// -> Network is ready

	if (Context->IsState(PCGExGraph::State_WritingIslands))
	{
		Context->IslandsIO->Flush();
		Context->Network->PrepareIslands(Context->MinIslandSize, Context->MaxIslandSize);
		Context->Markings->Mark = Context->CurrentIO->GetIn()->GetUniqueID();

		if (Context->bPruneIsolatedPoints)
		{
			UPCGPointData* OutData = Context->CurrentIO->GetOut();
			TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
			const int32 NumMaxNodes = Context->Network->Nodes.Num();
			MutablePoints.Reserve(NumMaxNodes);
			Context->IndexRemap.Empty();
			Context->IndexRemap.Reserve(NumMaxNodes);
			int32 Index = 0;
			for (PCGExGraph::FNode Node : Context->Network->Nodes)
			{
				if (Node.Island == -1 || Node.Edges.IsEmpty()) { continue; }
				if (*Context->Network->IslandSizes.Find(Node.Island) == -1) { continue; }

				Context->IndexRemap.Add(Node.Index, Index++);
				MutablePoints.Add(Context->CurrentIO->GetInPoint(Node.Index));
			}
		}


		//if (Context->bOutputIndividualIslands){
		for (const TPair<int32, int32>& Pair : Context->Network->IslandSizes)
		{
			const int32 IslandSize = Pair.Value;
			if (IslandSize == -1) { continue; }

			PCGExData::FPointIO& IslandIO = Context->IslandsIO->Emplace_GetRef(PCGExData::EInit::NewOutput);
			Context->Markings->Add(IslandIO);

			Context->GetAsyncManager()->Start<FWriteIslandTask>(
				Pair.Key, Context->CurrentIO, &IslandIO,
				Context->Network, Context->bPruneIsolatedPoints ? &Context->IndexRemap : nullptr);
		}

		if (Context->IslandsIO->IsEmpty())
		{
			Context->CurrentIO->GetOut()->Metadata->DeleteAttribute(PCGExGraph::PUIDAttributeName); // Unmark
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
		else
		{
			Context->SetAsyncState(PCGExGraph::State_WaitingOnWritingIslands);
		}
	}

	if (Context->IsState(PCGExGraph::State_WaitingOnWritingIslands))
	{
		if (Context->IsAsyncWorkComplete())
		{
			Context->Markings->UpdateMark();
			Context->IslandsIO->OutputTo(Context, true);
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	if (Context->IsDone())
	{
		if (Settings->bDeleteGraphData)
		{
			Context->MainPoints->ForEach(
				[&](PCGExData::FPointIO& PointIO, int32)
				{
					auto DeleteSockets = [&](const UPCGExGraphParamsData* Params, int32)
					{
						UPCGPointData* OutData = PointIO.GetOut();
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

bool FWriteIslandTask::ExecuteTask()
{
	const int32 IslandUID = TaskIndex;
	const int32 IslandSize = *Network->IslandSizes.Find(IslandUID);

	TSet<int32> IslandSet;
	TQueue<int32> Island;
	IslandSet.Reserve(IslandSize);

	for (PCGExGraph::FNode& Node : Network->Nodes)
	{
		if (Node.Island == -1 || Node.Island != IslandUID) { continue; }
		for (const int32 Edge : Node.Edges)
		{
			if (!IslandSet.Contains(Edge))
			{
				Island.Enqueue(Edge);
				IslandSet.Add(Edge);
			}
		}
	}

	IslandSet.Empty();

	TArray<FPCGPoint>& MutablePoints = IslandIO->GetOut()->GetMutablePoints();
	MutablePoints.SetNum(IslandSize);

	IslandIO->CreateOutKeys();

	PCGEx::TFAttributeWriter<int32>* EdgeStart = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::EdgeStartAttributeName, -1, false);
	PCGEx::TFAttributeWriter<int32>* EdgeEnd = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::EdgeEndAttributeName, -1, false);

	EdgeStart->BindAndGet(*IslandIO);
	EdgeEnd->BindAndGet(*IslandIO);

	int32 PointIndex = 0;
	int32 EdgeIndex;

	const TArray<FPCGPoint> Vertices = PointIO->GetIn() ? PointIO->GetIn()->GetPoints() : PointIO->GetOut()->GetPoints();

	if (IndexRemap)
	{
		while (Island.Dequeue(EdgeIndex))
		{
			const PCGExGraph::FUnsignedEdge& Edge = Network->Edges[EdgeIndex];
			MutablePoints[PointIndex].Transform.SetLocation(
				FMath::Lerp(
					Vertices[(EdgeStart->Values[PointIndex] = *IndexRemap->Find(Edge.Start))].Transform.GetLocation(),
					Vertices[(EdgeEnd->Values[PointIndex] = *IndexRemap->Find(Edge.End))].Transform.GetLocation(), 0.5));
			PointIndex++;
		}
	}
	else
	{
		while (Island.Dequeue(EdgeIndex))
		{
			const PCGExGraph::FUnsignedEdge& Edge = Network->Edges[EdgeIndex];
			MutablePoints[PointIndex].Transform.SetLocation(
				FMath::Lerp(
					Vertices[(EdgeStart->Values[PointIndex] = Edge.Start)].Transform.GetLocation(),
					Vertices[(EdgeEnd->Values[PointIndex] = Edge.End)].Transform.GetLocation(), 0.5));
			PointIndex++;
		}
	}


	EdgeStart->Write();
	EdgeEnd->Write();

	//IslandData->Cleanup();
	PCGEX_DELETE(EdgeStart)
	PCGEX_DELETE(EdgeEnd)

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
