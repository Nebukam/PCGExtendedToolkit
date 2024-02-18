// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExPromoteEdges.h"

#include "Graph/Edges/Promoting/PCGExEdgePromoteToPath.h"
#include "Graph/Edges/Promoting/PCGExEdgePromoteToPoint.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"
#define PCGEX_NAMESPACE PromoteEdges

int32 UPCGExPromoteEdgesSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_XS; }

PCGExData::EInit UPCGExPromoteEdgesSettings::GetMainOutputInitMode() const
{
	return Promotion && Promotion->GeneratesNewPointData() ?
		       PCGExData::EInit::NoOutput :
		       PCGExData::EInit::NewOutput;
}

void UPCGExPromoteEdgesSettings::PostInitProperties()
{
	Super::PostInitProperties();
	PCGEX_OPERATION_DEFAULT(Promotion, UPCGExEdgePromoteToPoint)
}

TArray<FPCGPinProperties> UPCGExPromoteEdgesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PinProperties.Pop();
	return PinProperties;
}

FName UPCGExPromoteEdgesSettings::GetMainOutputLabel() const { return PCGExGraph::OutputPathsLabel; }

PCGEX_INITIALIZE_ELEMENT(PromoteEdges)

bool FPCGExPromoteEdgesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExCustomGraphProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PromoteEdges)

	Context->EdgeCrawlingSettings = Settings->EdgeTypesSettings;

	PCGEX_OPERATION_BIND(Promotion, UPCGExEdgePromoteToPoint)

	return true;
}

bool FPCGExPromoteEdgesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPromoteEdgesElement::Execute);

	PCGEX_CONTEXT(PromoteEdges)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		Context->MaxPossibleEdgesPerPoint = 0;
		for (const UPCGExGraphDefinition* Graph : Context->Graphs.Params)
		{
			Context->MaxPossibleEdgesPerPoint += Graph->GetSocketMapping()->NumSockets;
		}

		if (Context->Promotion->GeneratesNewPointData())
		{
			int32 MaxPossibleOutputs = 0;
			for (const PCGExData::FPointIO* PointIO : Context->MainPoints->Pairs)
			{
				MaxPossibleOutputs += PointIO->GetNum();
			}

			MaxPossibleOutputs *= Context->MaxPossibleEdgesPerPoint;
			Context->OutputData.TaggedData.Reserve(MaxPossibleOutputs);
		}

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIOAndResetGraph()) { Context->Done(); }
		else
		{
			const int32 MaxNumEdges = (Context->MaxPossibleEdgesPerPoint * Context->CurrentIO->GetNum()) / 2; // Oof
			Context->Edges.Reset(MaxNumEdges);
			Context->UniqueEdges.Reset();
			Context->UniqueEdges.Reserve(MaxNumEdges);

			Context->CurrentIO->CreateInKeys();

			Context->SetState(PCGExGraph::State_ReadyForNextGraph);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph())
		{
			Context->SetState(PCGExGraph::State_PromotingEdges);
			return false;
		}

		if (!Context->PrepareCurrentGraphForPoints(*Context->CurrentIO))
		{
			PCGEX_GRAPH_MISSING_METADATA
			return false;
		}

		Context->SetState(PCGExGraph::State_ProcessingGraph);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingGraph))
	{
		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			const int32 EdgeType = Context->CurrentGraphEdgeCrawlingTypes;

			TArray<PCGExGraph::FUnsignedEdge> Edges;
			for (const PCGExGraph::FSocketInfos& SocketInfo : Context->SocketInfos)
			{
				const int32 End = SocketInfo.Socket->GetTargetIndexReader().Values[PointIndex];
				const int32 InEdgeType = SocketInfo.Socket->GetEdgeTypeReader().Values[PointIndex];
				if (End == -1 || (InEdgeType & EdgeType) == 0 || PointIndex == End) { continue; }

				uint64 Hash = PCGEx::H64U(PointIndex, End);
				{
					FReadScopeLock ReadLock(Context->EdgeLock);
					if (Context->UniqueEdges.Contains(Hash)) { continue; }
				}

				FWriteScopeLock WriteLock(Context->EdgeLock);
				Context->UniqueEdges.Add(Hash);
				Context->Edges.Emplace(PointIndex, End);
			}
		};

		if (!Context->ProcessCurrentPoints(ProcessPoint)) { return false; }

		Context->SetState(PCGExGraph::State_ReadyForNextGraph);
	}

	if (Context->IsState(PCGExGraph::State_PromotingEdges))
	{
		auto ProcessEdge = [&](const int32 Index)
		{
			const PCGExGraph::FUnsignedEdge& UEdge = Context->Edges[Index];
			Context->Promotion->PromoteEdge(
				UEdge,
				Context->CurrentIO->GetInPoint(UEdge.Start),
				Context->CurrentIO->GetInPoint(UEdge.End));
		};

		auto ProcessEdgeGen = [&](const int32 Index)
		{
			const PCGExGraph::FUnsignedEdge& UEdge = Context->Edges[Index];


			UPCGPointData* OutData = NewObject<UPCGPointData>();
			OutData->InitializeFromData(Context->CurrentIO->GetIn());

			if (bool bSuccess = Context->Promotion->PromoteEdgeGen(
				OutData,
				UEdge,
				Context->CurrentIO->GetInPoint(UEdge.Start),
				Context->CurrentIO->GetInPoint(UEdge.End)))
			{
				Context->EdgeLock.WriteLock();
				FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Emplace_GetRef();
				OutputRef.Data = OutData;
				OutputRef.Pin = Context->CurrentIO->DefaultOutputLabel;
				Context->EdgeLock.WriteUnlock();
			}
			else
			{
				OutData->ConditionalBeginDestroy(); // Ugh
			}
		};

		if (Context->Promotion->GeneratesNewPointData()) { if (!Context->Process(ProcessEdgeGen, Context->Edges.Num())) { return false; } }
		else { if (!Context->Process(ProcessEdge, Context->Edges.Num())) { return false; } }

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->UniqueEdges.Empty();
		Context->Edges.Empty();

		if (!Context->Promotion->GeneratesNewPointData())
		{
			Context->OutputPoints();
		}
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
