// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFindEdgesType.h"

#define LOCTEXT_NAMESPACE "PCGExFindEdgesType"

int32 UPCGExFindEdgesTypeSettings::GetPreferredChunkSize() const { return 32; }

PCGExIO::EInitMode UPCGExFindEdgesTypeSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::DuplicateInput; }

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

		if (Context->MainPoints->IsEmpty())
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingPoints", "Missing Input Points."));
			return true;
		}

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO(true)) { Context->Done(); }
		else
		{
			Context->CurrentIO->BuildMetadataEntries();
			Context->SetState(PCGExGraph::State_ReadyForNextGraph);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph())
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}
		Context->SetState(PCGExGraph::State_FindingEdgeTypes);
	}

	if (Context->IsState(PCGExGraph::State_FindingEdgeTypes))
	{
		auto Initialize = [&](const UPCGExPointIO* PointIO)
		{
			Context->PrepareCurrentGraphForPoints(PointIO->Out, true);
		};

		auto ProcessPoint = [&Context](const int32 PointIndex, const UPCGExPointIO* PointIO)
		{
			PCGExGraph::ComputeEdgeType(Context->SocketInfos, PointIO->GetOutPoint(PointIndex), PointIndex, PointIO);
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint))
		{
			Context->SetState(PCGExGraph::State_ReadyForNextGraph);
		}
	}

	if (Context->IsDone())
	{
		Context->OutputPointsAndParams();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
