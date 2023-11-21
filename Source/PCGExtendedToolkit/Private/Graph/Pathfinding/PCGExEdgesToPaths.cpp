// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExEdgesToPaths.h"

#include "Data/PCGSpatialData.h"
#include "PCGContext.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Graph/PCGExGraphHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"

int32 UPCGExEdgesToPathsSettings::GetPreferredChunkSize() const { return 32; }

PCGEx::EIOInit UPCGExEdgesToPathsSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

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
	return Context;
}

void FPCGExEdgesToPathsElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExGraphProcessorElement::InitializeContext(InContext, InputData, SourceComponent, Node);
	//FPCGExEdgesToPathsContext* Context = static_cast<FPCGExEdgesToPathsContext*>(InContext);
	// ...
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
			Context->CurrentIO->BuildMetadataEntries();
			Context->SetState(PCGExMT::EState::ReadyForNextGraph);
		}
	}

	auto ProcessPoint = [&Context](
		const FPCGPoint& Point, int32 ReadIndex, UPCGExPointIO* IO)
	{
		Context->ComputeEdgeType(Point, ReadIndex, IO);
	};

	if (Context->IsState(PCGExMT::EState::ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph())
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
			return false;
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingGraph);
		}
	}

	auto Initialize = [&Context](const UPCGExPointIO* IO)
	{
		Context->CurrentGraph->PrepareForPointData(Context, IO->Out, true);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingGraph))
	{
		if (Context->CurrentIO->OutputParallelProcessing(Context, Initialize, ProcessPoint, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextGraph);
		}
	}

	if (Context->IsState(PCGExMT::EState::Done))
	{
		Context->OutputPointsAndParams();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
