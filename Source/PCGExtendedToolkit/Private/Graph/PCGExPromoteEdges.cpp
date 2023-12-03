// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "..\..\Public\Graph\PCGExPromoteEdges.h"

#include "Graph/Promotions/PCGExEdgePromoteToPath.h"
#include "Graph/Promotions/PCGExEdgePromoteToPoint.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"

int32 UPCGExPromoteEdgesSettings::GetPreferredChunkSize() const { return 32; }

PCGExPointIO::EInit UPCGExPromoteEdgesSettings::GetPointOutputInitMode() const
{
	return Promotion && Promotion->GeneratesNewPointData() ?
		       PCGExPointIO::EInit::NoOutput :
		       PCGExPointIO::EInit::NewOutput;
}

UPCGExPromoteEdgesSettings::UPCGExPromoteEdgesSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Promotion = EnsureInstruction<UPCGExEdgePromoteToPoint>(Promotion);
}

TArray<FPCGPinProperties> UPCGExPromoteEdgesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PinProperties.Pop();
	return PinProperties;
}

FPCGElementPtr UPCGExPromoteEdgesSettings::CreateElement() const { return MakeShared<FPCGExPromoteEdgesElement>(); }

FName UPCGExPromoteEdgesSettings::GetMainPointsOutputLabel() const { return PCGExGraph::OutputPathsLabel; }

FPCGContext* FPCGExPromoteEdgesElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExPromoteEdgesContext* Context = new FPCGExPromoteEdgesContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExPromoteEdgesSettings* Settings = Context->GetInputSettings<UPCGExPromoteEdgesSettings>();
	check(Settings);

	Context->AsyncEdgesLoop = Context->MakeLoop();

	Context->EdgeType = static_cast<EPCGExEdgeType>(Settings->EdgeType);
	Context->Promotion = Settings->EnsureInstruction<UPCGExEdgePromoteToPoint>(Settings->Promotion, Context);
	return Context;
}

bool FPCGExPromoteEdgesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExEdgesToPathsElement::Execute);

	FPCGExPromoteEdgesContext* Context = static_cast<FPCGExPromoteEdgesContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }

		Context->MaxPossibleEdgesPerPoint = 0;
		for (const UPCGExGraphParamsData* Graph : Context->Graphs.Params)
		{
			Context->MaxPossibleEdgesPerPoint += Graph->GetSocketMapping()->NumSockets;
		}

		if (Context->Promotion->GeneratesNewPointData())
		{
			int32 MaxPossibleOutputs = 0;
			for (const UPCGExPointIO* PointIO : Context->MainPoints->Pairs)
			{
				MaxPossibleOutputs += PointIO->NumInPoints;
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
			const int32 MaxNumEdges = (Context->MaxPossibleEdgesPerPoint * Context->CurrentIO->NumInPoints) / 2; // Oof
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
			Context->AsyncEdgesLoop.NumIterations = Context->Edges.Num(); // !
			Context->SetState(PCGExGraph::State_PromotingEdges);
			return false;
		}
		Context->SetState(PCGExGraph::State_ProcessingGraph);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingGraph))
	{
		auto Initialize = [&](const UPCGExPointIO* PointIO)
		{
			Context->PrepareCurrentGraphForPoints(PointIO->In, true);
		};

		auto ProcessPoint = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
		{
			const FPCGPoint& Point = PointIO->GetInPoint(PointIndex);
			TArray<PCGExGraph::FUnsignedEdge> UnsignedEdges;
			Context->CurrentGraph->GetEdges(PointIndex, Point.MetadataEntry, UnsignedEdges, Context->EdgeType);

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
			OutData->InitializeFromData(Context->CurrentIO->In);

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
			if (Context->AsyncEdgesLoop.Advance(ProcessEdgeGen)) { Context->SetState(PCGExMT::State_ReadyForNextPoints); }
		}
		else
		{
			if (Context->AsyncEdgesLoop.Advance(ProcessEdge)) { Context->SetState(PCGExMT::State_ReadyForNextPoints); }
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

		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
