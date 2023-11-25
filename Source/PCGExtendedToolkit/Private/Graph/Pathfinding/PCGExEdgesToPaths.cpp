// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExEdgesToPaths.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"

int32 UPCGExEdgesToPathsSettings::GetPreferredChunkSize() const { return 32; }

PCGExIO::EInitMode UPCGExEdgesToPathsSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::NoOutput; }

FPCGElementPtr UPCGExEdgesToPathsSettings::CreateElement() const
{
	return MakeShared<FPCGExEdgesToPathsElement>();
}

FPCGContext* FPCGExEdgesToPathsElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExEdgesToPathsContext* Context = new FPCGExEdgesToPathsContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExEdgesToPathsSettings* Settings = Context->GetInputSettings<UPCGExEdgesToPathsSettings>();
	check(Settings);

	Context->EdgeType = static_cast<EPCGExEdgeType>(Settings->EdgeType);
	Context->bWriteTangents = Settings->bWriteTangents;
	Context->TangentParams = Settings->TangentParams;

	return Context;
}

bool FPCGExEdgesToPathsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExEdgesToPathsElement::Execute);

	FPCGExEdgesToPathsContext* Context = static_cast<FPCGExEdgesToPathsContext*>(InContext);

	if (Context->IsSetup())
	{
		if (Context->Params.IsEmpty())
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingParams", "Missing Input Params."));
			return true;
		}

		if (Context->Points->IsEmpty())
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingPoints", "Missing Input Points."));
			return true;
		}

		Context->EdgesIO = NewObject<UPCGExPointIOGroup>();
		Context->SetState(PCGExMT::EState::ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO(true))
		{
			Context->SetState(PCGExMT::EState::Done); //No more points
		}
		else
		{
			Context->SetState(PCGExMT::EState::ReadyForNextGraph);
		}
	}

	auto ProcessPointInGraph = [&](const FPCGPoint& Point, const int32 ReadIndex, const UPCGExPointIO* PointIO)
	{
		TArray<PCGExGraph::FUnsignedEdge> UnsignedEdges;
		Context->CurrentGraph->GetEdges(ReadIndex, Point.MetadataEntry, UnsignedEdges);
		FWriteScopeLock ScopeLock(Context->ContextLock);
		for (const PCGExGraph::FUnsignedEdge& UEdge : UnsignedEdges)
		{
			if (static_cast<uint8>((UEdge.Type & static_cast<EPCGExEdgeType>(Context->EdgeType))) == 0) { continue; }
			Context->UniqueEdges.AddUnique(UEdge); // Looks terrible but ended up performing better than a map
		}
	};

	if (Context->IsState(PCGExMT::EState::ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph())
		{
			Context->SetState(PCGExMT::EState::WaitingOnAsyncTasks);
			return false;
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingGraph);
		}
	}

	auto Initialize = [&](const UPCGExPointIO* PointIO)
	{
		Context->PrepareCurrentGraphForPoints(PointIO->In, true);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingGraph))
	{
		if (Context->CurrentIO->InputParallelProcessing(Context, Initialize, ProcessPointInGraph, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextGraph);
		}
	}

	auto ProcessEdge = [&](const int32 Index)
	{
		//const UPCGExPointIO* PointIO = Context->EdgesIO->Emplace_GetRef(*Context->CurrentIO, PCGExIO::EInitMode::NewOutput);
		const PCGExGraph::FUnsignedEdge& UEdge = Context->UniqueEdges[Index];

		UPCGPointData* Out = NewObject<UPCGPointData>();
		Out->InitializeFromData(Context->CurrentIO->In);
				
		FPCGPoint& Start = Out->GetMutablePoints().Emplace_GetRef(Context->CurrentIO->In->GetPoint(UEdge.Start));
		FPCGPoint& End = Out->GetMutablePoints().Emplace_GetRef(Context->CurrentIO->In->GetPoint(UEdge.End));

		if(Context->bWriteTangents)
		{
			Out->Metadata->InitializeOnSet(Start.MetadataEntry);
			Out->Metadata->InitializeOnSet(End.MetadataEntry);
			
			FVector StartIn = Start.Transform.GetRotation().GetForwardVector();
			FVector StartOut = StartIn*-1;
			FVector EndIn = End.Transform.GetRotation().GetForwardVector();
			FVector EndOut = EndIn*-1;

			Context->TangentParams.CreateAttributes(Out, Start, End,
				StartIn, StartOut, EndIn, EndOut);
			
		}

		FWriteScopeLock ScopeLock(Context->ContextLock);
		FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Emplace_GetRef();
		OutputRef.Data = Out;
		OutputRef.Pin = Context->EdgesIO->DefaultOutputLabel;
		
	};

	auto InitializeAsync = [&]()
	{
		
	};

	if (Context->IsState(PCGExMT::EState::WaitingOnAsyncTasks))
	{
		if (PCGExMT::ParallelForLoop(Context, Context->UniqueEdges.Num(), InitializeAsync, ProcessEdge, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
		}
	}

	if (Context->IsState(PCGExMT::EState::Done))
	{
		Context->UniqueEdges.Empty();
		Context->EdgesIO->OutputTo(Context);
		Context->OutputGraphParams();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
