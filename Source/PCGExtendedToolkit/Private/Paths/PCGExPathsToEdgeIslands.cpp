﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathsToEdgeIslands.h"

#include "Data/PCGExData.h"
#include "Graph/PCGExFindEdgeIslands.h"

#define LOCTEXT_NAMESPACE "PCGExPathsToEdgeIslandsElement"
#define PCGEX_NAMESPACE BuildGraph

namespace PCGExGraph
{
}

UPCGExPathsToEdgeIslandsSettings::UPCGExPathsToEdgeIslandsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPCGExPathsToEdgeIslandsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinIslandsOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinIslandsOutput.Tooltip = FTEXT("Point data representing edges.");
#endif // WITH_EDITOR

	//PCGEx::Swap(PinProperties, PinProperties.Num() - 1, PinProperties.Num() - 2);
	return PinProperties;
}

#if WITH_EDITOR
void UPCGExPathsToEdgeIslandsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGExData::EInit UPCGExPathsToEdgeIslandsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FName UPCGExPathsToEdgeIslandsSettings::GetMainInputLabel() const { return PCGExGraph::SourcePathsLabel; }

FName UPCGExPathsToEdgeIslandsSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(PathsToEdgeIslands)

FPCGExPathsToEdgeIslandsContext::~FPCGExPathsToEdgeIslandsContext()
{
	PCGEX_TERMINATE_ASYNC

	VisitedNodes.Empty();

	PCGEX_DELETE(Markings)
	PCGEX_DELETE(IslandsIO)

	PCGEX_DELETE(LooseNetwork)
	PCGEX_DELETE(EdgeNetwork)
	PCGEX_DELETE(EdgeCrossings)
}

bool FPCGExPathsToEdgeIslandsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathsToEdgeIslands)

	Context->LooseNetwork = new PCGExGraph::FLooseNetwork(Settings->FuseDistance);
	Context->IOIndices.Empty();

	PCGEX_FWD(bFindCrossings)
	PCGEX_FWD(CrossingTolerance)

	return true;
}


bool FPCGExPathsToEdgeIslandsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathsToEdgeIslandsElement::Execute);

	PCGEX_CONTEXT(PathsToEdgeIslands)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->MainPoints->ForEach(
			[&](PCGExData::FPointIO& PointIO, const int32 Index)
			{
				Context->IOIndices.Add(&PointIO, Index);
			});

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->ConsolidatedPoints = &Context->MainPoints->Emplace_GetRef();
			Context->ConsolidatedPoints->GetOut()->GetMutablePoints().SetNum(Context->LooseNetwork->Nodes.Num());

			TArray<FPCGPoint>& MutablePoints = Context->ConsolidatedPoints->GetOut()->GetMutablePoints();
			for (int i = 0; i < MutablePoints.Num(); i++)
			{
				MutablePoints[i].Transform.SetLocation(Context->LooseNetwork->Nodes[i]->Center);
			}

			Context->IslandsIO = new PCGExData::FPointIOGroup();
			Context->IslandsIO->DefaultOutputLabel = PCGExGraph::OutputEdgesLabel;

			Context->EdgeNetwork = new PCGExGraph::FEdgeNetwork(Context->ConsolidatedPoints->GetNum() * 2, Context->ConsolidatedPoints->GetNum());
			Context->Markings = new PCGExData::FKPointIOMarkedBindings<int32>(Context->ConsolidatedPoints, PCGExGraph::PUIDAttributeName);

			Context->SetState(PCGExGraph::State_ProcessingGraph);
		}
		else { Context->SetState(PCGExMT::State_ProcessingPoints); }
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto Initialize = [&](PCGExData::FPointIO& PointIO)
		{
		};

		auto ProcessPoint = [&](const int32 Index, const PCGExData::FPointIO& PointIO)
		{
			if (PointIO.GetNum() < 2) { return; }

			const int32 PrevIndex = Index - 1;
			const int32 NextIndex = Index + 1;

			PCGExGraph::FLooseNode* CurrentVtx = Context->LooseNetwork->GetLooseNode(PointIO.GetInPoint(Index));
			CurrentVtx->Add(static_cast<uint64>(*Context->IOIndices.Find(&PointIO)) | (static_cast<uint64>(Index) << 32));

			if (PointIO.GetIn()->GetPoints().IsValidIndex(PrevIndex))
			{
				PCGExGraph::FLooseNode* OtherVtx = Context->LooseNetwork->GetLooseNode(PointIO.GetInPoint(PrevIndex));
				CurrentVtx->Add(OtherVtx);
			}

			if (PointIO.GetIn()->GetPoints().IsValidIndex(NextIndex))
			{
				PCGExGraph::FLooseNode* OtherVtx = Context->LooseNetwork->GetLooseNode(PointIO.GetInPoint(NextIndex));
				CurrentVtx->Add(OtherVtx);
			}
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint, true))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	if (Context->IsState(PCGExGraph::State_ProcessingGraph))
	{
		// Build Network
		Context->VisitedNodes.Empty();
		Context->VisitedNodes.Reserve(Context->LooseNetwork->Nodes.Num());

		for (const PCGExGraph::FLooseNode* Node : Context->LooseNetwork->Nodes)
		{
			TQueue<int32> Queue;
			Queue.Enqueue(Node->Index);

			int32 Index;
			while (Queue.Dequeue(Index))
			{
				if (!Context->VisitedNodes.Contains(Index))
				{
					Context->VisitedNodes.Add(Index);

					for (PCGExGraph::FLooseNode* OtherNode = Context->LooseNetwork->Nodes[Index];
					     const int32 OtherNodeIndex : OtherNode->Neighbors)
					{
						Context->EdgeNetwork->InsertEdge(PCGExGraph::FUnsignedEdge(Index, OtherNodeIndex, EPCGExEdgeType::Complete));
						Queue.Enqueue(OtherNodeIndex);
					}
				}
			}
		}

		if (Context->bFindCrossings)
		{
			Context->EdgeCrossings = new PCGExGraph::FEdgeCrossingsHandler(Context->EdgeNetwork, Context->CrossingTolerance);
			Context->EdgeCrossings->Prepare(Context->ConsolidatedPoints->GetOut()->GetPoints());
			Context->SetState(PCGExGraph::State_FindingCrossings);
		}
		else
		{
			Context->SetState(PCGExGraph::State_WritingIslands);
		}
	}

	if (Context->IsState(PCGExGraph::State_FindingCrossings))
	{
		auto Initialize = [&]()
		{
			Context->EdgeCrossings->Prepare(Context->ConsolidatedPoints->GetOut()->GetPoints());
		};

		auto ProcessEdge = [&](const int32 Index)
		{
			Context->EdgeCrossings->ProcessEdge(Index, Context->ConsolidatedPoints->GetOut()->GetPoints());
		};

		if (Context->Process(Initialize, ProcessEdge, Context->EdgeNetwork->Edges.Num()))
		{
			Context->EdgeCrossings->InsertCrossings();

			TArray<FPCGPoint>& MutablePoints = Context->ConsolidatedPoints->GetOut()->GetMutablePoints();
			MutablePoints.Reserve(MutablePoints.Num() + Context->EdgeCrossings->Crossings.Num());

			for (const PCGExGraph::FEdgeCrossing& Crossing : Context->EdgeCrossings->Crossings)
			{
				MutablePoints.Emplace_GetRef().Transform.SetLocation(Crossing.Center);
			}

			Context->SetState(PCGExGraph::State_WritingIslands);
		}
	}

	if (Context->IsState(PCGExGraph::State_WritingIslands))
	{
		Context->VisitedNodes.Empty();

		Context->IslandsIO->Flush();
		Context->EdgeNetwork->PrepareIslands(); // !
		Context->Markings->Mark = Context->ConsolidatedPoints->GetOut()->GetUniqueID();

		for (const TPair<int32, int32>& Pair : Context->EdgeNetwork->IslandSizes)
		{
			const int32 IslandSize = Pair.Value;
			if (IslandSize == -1) { continue; }

			PCGExData::FPointIO& IslandIO = Context->IslandsIO->Emplace_GetRef(PCGExData::EInit::NewOutput);
			Context->Markings->Add(IslandIO);

			Context->GetAsyncManager()->Start<FWriteIslandTask>(Pair.Key, Context->ConsolidatedPoints, &IslandIO, Context->EdgeNetwork);
		}

		Context->SetAsyncState(PCGExGraph::State_WaitingOnWritingIslands);
	}

	if (Context->IsState(PCGExGraph::State_WaitingOnWritingIslands))
	{
		if (Context->IsAsyncWorkComplete())
		{
			Context->Markings->UpdateMark();
			Context->IslandsIO->OutputTo(Context, true);
			Context->ConsolidatedPoints->OutputTo(Context, true);
			Context->Done();
		}
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
