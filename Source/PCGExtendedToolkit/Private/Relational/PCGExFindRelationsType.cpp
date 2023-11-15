// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "..\..\Public\Relational\PCGExFindRelationsType.h"

#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Relational/PCGExRelationsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExFindRelationsType"

int32 UPCGExFindRelationsTypeSettings::GetPreferredChunkSize() const { return 32; }

PCGEx::EIOInit UPCGExFindRelationsTypeSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

FPCGElementPtr UPCGExFindRelationsTypeSettings::CreateElement() const
{
	return MakeShared<FPCGExFindRelationsTypeElement>();
}

FPCGContext* FPCGExFindRelationsTypeElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExFindRelationsTypeContext* Context = new FPCGExFindRelationsTypeContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

void FPCGExFindRelationsTypeElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExRelationsProcessorElement::InitializeContext(InContext, InputData, SourceComponent, Node);
	//FPCGExFindRelationsTypeContext* Context = static_cast<FPCGExFindRelationsTypeContext*>(InContext);
	// ...
}

bool FPCGExFindRelationsTypeElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindRelationsTypeElement::Execute);

	FPCGExFindRelationsTypeContext* Context = static_cast<FPCGExFindRelationsTypeContext*>(InContext);

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
			Context->SetState(PCGExMT::EState::ReadyForNextParams);
		}
	}

	auto ProcessPoint = [&Context](
		const FPCGPoint& Point, int32 ReadIndex, UPCGExPointIO* IO)
	{
		Context->ComputeRelationsType(Point, ReadIndex, IO);
	};

	if (Context->IsState(PCGExMT::EState::ReadyForNextParams))
	{
		if (!Context->AdvanceParams())
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
			return false;
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingParams);
		}
	}

	auto Initialize = [&Context](UPCGExPointIO* IO)
	{
		Context->CurrentParams->PrepareForPointData(Context, IO->Out);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingParams))
	{
		if (Context->CurrentIO->OutputParallelProcessing(Context, Initialize, ProcessPoint, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextParams);
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
