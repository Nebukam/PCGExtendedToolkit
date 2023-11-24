// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExEdgesToPaths.h"

#include "Data/PCGSpatialData.h"
#include "PCGContext.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"

int32 UPCGExEdgesToPathsSettings::GetPreferredChunkSize() const { return 32; }

PCGEx::EIOInit UPCGExEdgesToPathsSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::NoOutput; }

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

	auto ProcessPointInGraph = [&Context](const FPCGPoint& Point, const int32 ReadIndex, const UPCGExPointIO* IO)
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

	auto Initialize = [&Context](const UPCGExPointIO* IO)
	{
		Context->CurrentGraph->PrepareForPointData(IO->In, true);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingGraph))
	{
		if (Context->CurrentIO->InputParallelProcessing(Context, Initialize, ProcessPointInGraph, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextGraph);
		}
	}

	auto ProcessEdge = [&Context](const int32 Index)
	{
		const UPCGExPointIO* IO = Context->EdgesIO->Emplace_GetRef(*Context->CurrentIO, PCGEx::EIOInit::NewOutput);
		const PCGExGraph::FUnsignedEdge& UEdge = Context->UniqueEdges[Index];

		FPCGPoint& Start = IO->Out->GetMutablePoints().Emplace_GetRef(IO->In->GetPoint(UEdge.Start));
		FPCGPoint& End = IO->Out->GetMutablePoints().Emplace_GetRef(IO->In->GetPoint(UEdge.End));

		if(Context->bWriteTangents)
		{
			IO->Out->Metadata->InitializeOnSet(Start.MetadataEntry);
			IO->Out->Metadata->InitializeOnSet(End.MetadataEntry);
			
			FVector StartIn = Start.Transform.GetRotation().GetForwardVector();
			FVector StartOut = StartIn*-1;
			FVector EndIn = End.Transform.GetRotation().GetForwardVector();
			FVector EndOut = EndIn*-1;

			Context->TangentParams.CreateAttributes(IO->Out, Start, End,
				StartIn, StartOut, EndIn, EndOut);
			
		}
	};

	auto InitializeAsync = [&Context]()
	{
		
	};

	if (Context->IsState(PCGExMT::EState::WaitingOnAsyncTasks))
	{
		if (PCGEx::Common::ParallelForLoop(Context, Context->UniqueEdges.Num(), InitializeAsync, ProcessEdge, Context->ChunkSize))
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
