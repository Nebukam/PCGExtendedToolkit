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

	auto ProcessPoint = [&Context](const int32 PointIndex, const UPCGExPointIO* PointIO)
	{
		Context->ComputeEdgeType(PointIO->GetOutPoint(PointIndex), PointIndex, PointIO);
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

	auto Initialize = [&](const UPCGExPointIO* PointIO)
	{
		Context->PrepareCurrentGraphForPoints(PointIO->Out, true);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingGraph))
	{
		if (Context->AsyncProcessingCurrentPoints(Initialize, ProcessPoint))
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
