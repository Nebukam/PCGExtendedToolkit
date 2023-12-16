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

FPCGElementPtr UPCGExFindEdgeIslandsSettings::CreateElement() const
{
	return MakeShared<FPCGExFindEdgeIslandsElement>();
}

void FPCGExFindEdgeIslandsContext::CenterEdgePoint(FPCGPoint& Point, PCGExGraph::FUnsignedEdge Edge) const
{
	Point.Transform.SetLocation(
		FMath::Lerp(
			CurrentIO->GetInPoint(Edge.Start).Transform.GetLocation(),
			CurrentIO->GetInPoint(Edge.End).Transform.GetLocation(), 0.5));
}

PCGEX_INITIALIZE_CONTEXT(FindEdgeIslands)

bool FPCGExFindEdgeIslandsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExGraphProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindEdgeIslands)

	Context->CrawlEdgeTypes = static_cast<EPCGExEdgeType>(Settings->CrawlEdgeTypes);

	PCGEX_FWD(bOutputIndividualIslands)
	PCGEX_FWD(bPruneIsolatedPoints)

	Context->MinIslandSize = Settings->bRemoveSmallIslands ? FMath::Max(1, Settings->MinIslandSize) : 1;
	Context->MaxIslandSize = Settings->bRemoveBigIslands ? FMath::Max(1, Settings->MaxIslandSize) : TNumericLimits<int32>::Max();

	PCGEX_FWD(IslandIDAttributeName)
	PCGEX_FWD(IslandSizeAttributeName)
	PCGEX_FWD(ResolveRoamingMethod)

	PCGEX_VALIDATE_NAME(Context->IslandIDAttributeName)
	PCGEX_VALIDATE_NAME(Context->IslandSizeAttributeName)

	return true;
}

bool FPCGExFindEdgeIslandsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindEdgeIslandsElement::Execute);

	PCGEX_CONTEXT(FindEdgeIslands)

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
		for (int i = 0; i < NumNodes; i++) { Context->Network->ProcessNode(i, VisitedNodes, Context->SocketInfos, EdgeType); }

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

		if (Context->bOutputIndividualIslands)
		{
			for (const TPair<int32, int32>& Pair : Context->Network->IslandSizes)
			{
				const int32 IslandSize = Pair.Value;
				if (IslandSize == -1) { continue; }

				PCGExData::FPointIO& IslandData = Context->IslandsIO->Emplace_GetRef(PCGExData::EInit::NewOutput);
				Context->Markings->Add(IslandData);

				Context->GetAsyncManager()->Start<FWriteIslandTask>(Pair.Key, Context->CurrentIO, &IslandData);
			}

			if (Context->IslandsIO->IsEmpty())
			{
				Context->CurrentIO->GetOut()->Metadata->DeleteAttribute(PCGExGraph::PUIDAttributeName); // Unmark
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
			}
			else { Context->SetAsyncState(PCGExGraph::State_WaitingOnWritingIslands); }
		}
		else
		{
			// Output all islands

			PCGExData::FPointIO& IslandData = Context->IslandsIO->Emplace_GetRef(PCGExData::EInit::NewOutput);

			TArray<FPCGPoint>& MutablePoints = IslandData.GetOut()->GetMutablePoints();
			MutablePoints.SetNum(Context->Network->NumEdges);

			IslandData.CreateOutKeys();

			PCGEx::TFAttributeWriter<int32>* EdgeStart = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::EdgeStartAttributeName, -1, false);
			PCGEx::TFAttributeWriter<int32>* EdgeEnd = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::EdgeEndAttributeName, -1, false);
			PCGEx::TFAttributeWriter<int32>* IslandID = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::IslandIndexAttributeName, -1, false);

			EdgeStart->BindAndGet(IslandData);
			EdgeEnd->BindAndGet(IslandData);
			IslandID->BindAndGet(IslandData);


			int32 PointIndex = 0;
			for (const PCGExGraph::FUnsignedEdge& Edge : Context->Network->Edges)
			{
				const int32 IslandSize = *Context->Network->IslandSizes.Find(Context->Network->Nodes[Edge.Start].Island);
				if (IslandSize == -1) { continue; }

				if (Context->bPruneIsolatedPoints)
				{
					EdgeStart->Values[PointIndex] = *Context->IndexRemap.Find(Edge.Start);
					EdgeEnd->Values[PointIndex] = *Context->IndexRemap.Find(Edge.End);
				}
				else
				{
					EdgeStart->Values[PointIndex] = Edge.Start;
					EdgeEnd->Values[PointIndex] = Edge.End;
				}

				IslandID->Values[PointIndex] = Context->Network->Nodes[Edge.Start].Island;
				Context->CenterEdgePoint(MutablePoints[PointIndex++], Edge);
			}

			EdgeStart->Write();
			EdgeEnd->Write();
			IslandID->Write();

			IslandData.Cleanup();
			PCGEX_DELETE(EdgeStart)
			PCGEX_DELETE(EdgeEnd)
			PCGEX_DELETE(IslandID)

			Context->SetState(PCGExGraph::State_WaitingOnWritingIslands);
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

	if (Context->IsDone()) { Context->OutputPoints(); }

	return Context->IsDone();
}

bool FInsertEdgeTask::ExecuteTask()
{
	const FPCGExFindEdgeIslandsContext* Context = Manager->GetContext<FPCGExFindEdgeIslandsContext>();
	PCGEX_ASYNC_CHECKPOINT

	FWriteScopeLock WriteLock(Context->NetworkLock);
	for (const PCGExGraph::FSocketInfos& SocketInfo : Context->SocketInfos)
	{
		if (PCGExGraph::FUnsignedEdge UEdge;
			SocketInfo.Socket->TryGetEdge(TaskIndex, UEdge, Context->CrawlEdgeTypes))
		{
			check(UEdge.Start != -1 && UEdge.End != -1)
			Context->Network->InsertEdge(UEdge);
		}
	}

	return true;
}

bool FWriteIslandTask::ExecuteTask()
{
	const FPCGExFindEdgeIslandsContext* Context = Manager->GetContext<FPCGExFindEdgeIslandsContext>();
	PCGEX_ASYNC_CHECKPOINT

	const int32 IslandUID = TaskIndex;
	const int32 IslandSize = *Context->Network->IslandSizes.Find(IslandUID);

	TSet<uint64> Island;
	Island.Reserve(IslandSize);

	for (PCGExGraph::FNode& Node : Context->Network->Nodes)
	{
		if (Node.Island == -1 || Node.Island != IslandUID) { continue; }
		for (const int32 Edge : Node.Edges) { Island.Add(Context->Network->Edges[Edge].GetUnsignedHash()); }
	}

	PCGEX_ASYNC_CHECKPOINT

	TArray<FPCGPoint>& MutablePoints = IslandData->GetOut()->GetMutablePoints();
	MutablePoints.SetNum(Island.Num());

	IslandData->CreateOutKeys();

	PCGEx::TFAttributeWriter<int32>* EdgeStart = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::EdgeStartAttributeName, -1, false);
	PCGEx::TFAttributeWriter<int32>* EdgeEnd = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::EdgeEndAttributeName, -1, false);

	EdgeStart->BindAndGet(*IslandData);
	EdgeEnd->BindAndGet(*IslandData);

	int32 PointIndex = 0;
	for (const uint64 EdgeHash : Island)
	{
		PCGExGraph::FUnsignedEdge Edge = PCGExGraph::FUnsignedEdge(EdgeHash);

		if (Context->bPruneIsolatedPoints)
		{
			EdgeStart->Values[PointIndex] = *Context->IndexRemap.Find(Edge.Start);
			EdgeEnd->Values[PointIndex] = *Context->IndexRemap.Find(Edge.End);
		}
		else
		{
			EdgeStart->Values[PointIndex] = Edge.Start;
			EdgeEnd->Values[PointIndex] = Edge.End;
		}

		Context->CenterEdgePoint(MutablePoints[PointIndex++], PCGExGraph::FUnsignedEdge(Edge));
	}

	PCGEX_ASYNC_CHECKPOINT

	EdgeStart->Write();
	EdgeEnd->Write();

	//IslandData->Cleanup();
	PCGEX_DELETE(EdgeStart)
	PCGEX_DELETE(EdgeEnd)

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
