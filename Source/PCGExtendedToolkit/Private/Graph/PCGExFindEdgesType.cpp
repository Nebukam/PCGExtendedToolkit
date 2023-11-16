// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFindEdgesType.h"

#include "Data/PCGSpatialData.h"
#include "PCGContext.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Graph/PCGExGraphHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExFindEdgesType"

int32 UPCGExFindEdgesTypeSettings::GetPreferredChunkSize() const { return 32; }

PCGEx::EIOInit UPCGExFindEdgesTypeSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

FPCGElementPtr UPCGExFindEdgesTypeSettings::CreateElement() const
{
	return MakeShared<FPCGExFindEdgesTypeElement>();
}

FPCGContext* FPCGExFindEdgesTypeElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExFindEdgesTypeContext* Context = new FPCGExFindEdgesTypeContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

void FPCGExFindEdgesTypeElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExGraphProcessorElement::InitializeContext(InContext, InputData, SourceComponent, Node);
	//FPCGExFindEdgesTypeContext* Context = static_cast<FPCGExFindEdgesTypeContext*>(InContext);
	// ...
}

bool FPCGExFindEdgesTypeElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindEdgesTypeElement::Execute);

	FPCGExFindEdgesTypeContext* Context = static_cast<FPCGExFindEdgesTypeContext*>(InContext);

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
		if (!Context->AdvanceParams())
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
		Context->CurrentParams->PrepareForPointData(Context, IO->Out, true);
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
