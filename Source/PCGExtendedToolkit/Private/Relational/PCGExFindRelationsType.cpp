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

	if (Context->IsCurrentOperation(PCGEx::EOperation::Setup))
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

		Context->SetOperation(PCGEx::EOperation::ReadyForNextPoints);
	}

	if (Context->IsCurrentOperation(PCGEx::EOperation::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO(true))
		{
			Context->SetOperation(PCGEx::EOperation::Done); //No more points
		}
		else
		{
			Context->CurrentIO->BuildMetadataEntries();
			Context->SetOperation(PCGEx::EOperation::ReadyForNextParams);
		}
	}

	auto ProcessPoint = [&Context](
		const FPCGPoint& Point, int32 ReadIndex, UPCGExPointIO* IO)
	{
		for (PCGExRelational::FSocketInfos& SocketInfos : Context->SocketInfos)
		{
			EPCGExRelationType Type = EPCGExRelationType::Unknown;
			const int64 RelationIndex = SocketInfos.Socket->GetRelationIndex(Point.MetadataEntry);

			if (RelationIndex != -1)
			{
				const int32 Key = IO->Out->GetPoint(RelationIndex).MetadataEntry;
				for (PCGExRelational::FSocketInfos& OtherSocketInfos : Context->SocketInfos)
				{
					if (OtherSocketInfos.Socket->GetRelationIndex(Key) == ReadIndex)
					{
						//TODO: Handle cases where there can be multiple sockets with a valid connection
						Type = PCGExRelational::Helpers::GetRelationType(SocketInfos, OtherSocketInfos);
					}
				}

				if (Type == EPCGExRelationType::Unknown) { Type = EPCGExRelationType::Unique; }
			}


			SocketInfos.Socket->SetRelationType(Point.MetadataEntry, Type);
		}
	};

	if (Context->IsCurrentOperation(PCGEx::EOperation::ReadyForNextParams))
	{
		if (!Context->AdvanceParams())
		{
			Context->SetOperation(PCGEx::EOperation::ReadyForNextPoints);
			return false;
		}
		else
		{
			Context->SetOperation(PCGEx::EOperation::ProcessingParams);
		}
	}

	auto Initialize = [&Context](UPCGExPointIO* IO)
	{
		Context->CurrentParams->PrepareForPointData(Context, IO->Out);
	};

	if (Context->IsCurrentOperation(PCGEx::EOperation::ProcessingParams))
	{
		if (Context->CurrentIO->OutputParallelProcessing(Context, Initialize, ProcessPoint, Context->ChunkSize))
		{
			Context->SetOperation(PCGEx::EOperation::ReadyForNextParams);
		}
	}

	if (Context->IsCurrentOperation(PCGEx::EOperation::Done))
	{
		Context->Points->OutputTo(Context);
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
