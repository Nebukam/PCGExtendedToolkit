// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExWriteEdgeExtras.h"

#include "Graph/Promotions/PCGExEdgePromoteToPath.h"
#include "Graph/Promotions/PCGExEdgePromoteToPoint.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"
#define PCGEX_NAMESPACE WriteEdgeExtras

int32 UPCGExWriteEdgeExtrasSettings::GetPreferredChunkSize() const { return 32; }

PCGExData::EInit UPCGExWriteEdgeExtrasSettings::GetMainOutputInitMode() const
{
	return Promotion && Promotion->GeneratesNewPointData() ?
		       PCGExData::EInit::NoOutput :
		       PCGExData::EInit::NewOutput;
}

UPCGExWriteEdgeExtrasSettings::UPCGExWriteEdgeExtrasSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Promotion = EnsureOperation<UPCGExEdgePromoteToPoint>(Promotion);
}

TArray<FPCGPinProperties> UPCGExWriteEdgeExtrasSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PinProperties.Pop();
	return PinProperties;
}

FPCGElementPtr UPCGExWriteEdgeExtrasSettings::CreateElement() const { return MakeShared<FPCGExWriteEdgeExtrasElement>(); }

FName UPCGExWriteEdgeExtrasSettings::GetMainOutputLabel() const { return PCGExGraph::OutputPathsLabel; }

PCGEX_INITIALIZE_CONTEXT(WriteEdgeExtras)

bool FPCGExWriteEdgeExtrasElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExGraphProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteEdgeExtras)

	Context->EdgeType = static_cast<EPCGExEdgeType>(Settings->EdgeType);

	PCGEX_BIND_OPERATION(Promotion, UPCGExEdgePromoteToPoint)

	return true;
}

bool FPCGExWriteEdgeExtrasElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteEdgeExtrasElement::Execute);

	PCGEX_CONTEXT(WriteEdgeExtras)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		Context->MaxPossibleEdgesPerPoint = 0;
		for (const UPCGExGraphParamsData* Graph : Context->Graphs.Params)
		{
			Context->MaxPossibleEdgesPerPoint += Graph->GetSocketMapping()->NumSockets;
		}

		if (Context->Promotion->GeneratesNewPointData())
		{
			int32 MaxPossibleOutputs = 0;
			for (const PCGExData::FPointIO& PointIO : Context->MainPoints->Pairs)
			{
				MaxPossibleOutputs += PointIO.GetNum();
			}

			MaxPossibleOutputs *= Context->MaxPossibleEdgesPerPoint;
			UE_LOG(LogTemp, Warning, TEXT("Max Possible Outputs = %d"), MaxPossibleOutputs);
			Context->OutputData.TaggedData.Reserve(MaxPossibleOutputs);
		}

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO(true)) { Context->Done(); }
		else
		{
			const int32 MaxNumEdges = (Context->MaxPossibleEdgesPerPoint * Context->CurrentIO->GetNum()) / 2; // Oof
			Context->Edges.Reset(MaxNumEdges);
			Context->UniqueEdges.Reset();
			Context->UniqueEdges.Reserve(MaxNumEdges);
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
		Context->SetState(PCGExGraph::State_ProcessingGraph);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingGraph))
	{
		auto Initialize = [&](const PCGExData::FPointIO& PointIO)
		{
			Context->PrepareCurrentGraphForPoints(PointIO);
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			const FPCGPoint& Point = PointIO.GetInPoint(PointIndex);
			TArray<PCGExGraph::FUnsignedEdge> UnsignedEdges;
			Context->CurrentGraph->GetEdges(PointIndex, UnsignedEdges, Context->EdgeType);

			for (const PCGExGraph::FUnsignedEdge& UEdge : UnsignedEdges)
			{
				Context->EdgeLock.ReadLock();
				if (Context->UniqueEdges.Contains(UEdge.GetUnsignedHash()))
				{
					Context->EdgeLock.ReadUnlock();
					continue;
				}
				Context->EdgeLock.ReadUnlock();

				Context->EdgeLock.WriteLock();
				Context->UniqueEdges.Add(UEdge.GetUnsignedHash());
				Context->Edges.Add(UEdge);
				Context->EdgeLock.WriteUnlock();
			}
		};


		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint))
		{
			Context->SetState(PCGExGraph::State_ReadyForNextGraph);
		}
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

			bool bSuccess = Context->Promotion->PromoteEdgeGen(
				OutData,
				UEdge,
				Context->CurrentIO->GetInPoint(UEdge.Start),
				Context->CurrentIO->GetInPoint(UEdge.End));

			if (bSuccess)
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

		if (Context->Promotion->GeneratesNewPointData())
		{
			if (Context->Process(ProcessEdgeGen, Context->Edges.Num())) { Context->SetState(PCGExMT::State_ReadyForNextPoints); }
		}
		else
		{
			if (Context->Process(ProcessEdge, Context->Edges.Num())) { Context->SetState(PCGExMT::State_ReadyForNextPoints); }
		}
	}

	if (Context->IsDone())
	{
		Context->UniqueEdges.Empty();
		Context->Edges.Empty();

		UE_LOG(LogTemp, Warning, TEXT("Actual Outputs = %d"), Context->OutputData.TaggedData.Num());

		if (!Context->Promotion->GeneratesNewPointData())
		{
			Context->OutputPoints();
		}
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
