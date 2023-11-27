// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExEdgesToPaths.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"

int32 UPCGExEdgesToPathsSettings::GetPreferredChunkSize() const { return 32; }
PCGExIO::EInitMode UPCGExEdgesToPathsSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::NoOutput; }
bool UPCGExEdgesToPathsSettings::GetRequiresSeeds() const { return false; }
bool UPCGExEdgesToPathsSettings::GetRequiresGoals() const { return false; }

FPCGElementPtr UPCGExEdgesToPathsSettings::CreateElement() const { return MakeShared<FPCGExEdgesToPathsElement>(); }

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
	return Context;
}

bool FPCGExEdgesToPathsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExEdgesToPathsElement::Execute);

	FPCGExEdgesToPathsContext* Context = static_cast<FPCGExEdgesToPathsContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
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

	auto ProcessPoint = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
	{
		const FPCGPoint& Point = PointIO->GetInPoint(PointIndex);
		TArray<PCGExGraph::FUnsignedEdge> UnsignedEdges;
		Context->CurrentGraph->GetEdges(PointIndex, Point.MetadataEntry, UnsignedEdges);

		for (const PCGExGraph::FUnsignedEdge& UEdge : UnsignedEdges)
		{
			{
				FReadScopeLock ReadLock(Context->EdgeLock);

				if (static_cast<uint8>((UEdge.Type & static_cast<EPCGExEdgeType>(Context->EdgeType))) == 0 ||
					Context->UniqueEdges.Contains(UEdge.GetUnsignedHash())) { continue; }
			}

			{
				FWriteScopeLock WriteLock(Context->EdgeLock);
				Context->UniqueEdges.Add(UEdge.GetUnsignedHash());
				Context->Edges.Add(UEdge);
			}
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
		if (Context->AsyncProcessingCurrentPoints(Initialize, ProcessPoint))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextGraph);
		}
	}

	auto ProcessEdge = [&](const int32 Index)
	{
		FWriteScopeLock WriteLock(Context->EdgeLock);

		//const UPCGExPointIO* PointIO = Context->EdgesIO->Emplace_GetRef(*Context->CurrentIO, PCGExIO::EInitMode::NewOutput);
		const PCGExGraph::FUnsignedEdge& UEdge = Context->Edges[Index];

		UPCGPointData* Out = NewObject<UPCGPointData>();
		Out->InitializeFromData(Context->CurrentIO->In);

		FPCGPoint& Start = Out->GetMutablePoints().Emplace_GetRef(Context->CurrentIO->In->GetPoints()[UEdge.Start]);
		FPCGPoint& End = Out->GetMutablePoints().Emplace_GetRef(Context->CurrentIO->In->GetPoints()[UEdge.End]);

		FPCGTaggedData& OutputRef = Context->OutputData.TaggedData.Emplace_GetRef();
		OutputRef.Data = Out;
		OutputRef.Pin = PCGExGraph::OutputPathsLabel;
	};

	if (Context->IsState(PCGExMT::EState::WaitingOnAsyncTasks))
	{
		if (PCGExMT::ParallelForLoop(Context, Context->Edges.Num(), ProcessEdge, Context->ChunkSize, !Context->bDoAsyncProcessing))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
		}
	}

	if (Context->IsState(PCGExMT::EState::Done))
	{
		Context->UniqueEdges.Empty();
		Context->Edges.Empty();
		//Context->OutputGraphParams();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
