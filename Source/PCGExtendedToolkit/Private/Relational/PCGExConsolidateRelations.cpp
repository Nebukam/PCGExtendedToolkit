// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Relational/PCGExConsolidateRelations.h"

#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Relational/PCGExRelationsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExConsolidateRelations"

int32 UPCGExConsolidateRelationsSettings::GetPreferredChunkSize() const { return 32; }

PCGEx::EIOInit UPCGExConsolidateRelationsSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

FPCGElementPtr UPCGExConsolidateRelationsSettings::CreateElement() const
{
	return MakeShared<FPCGExConsolidateRelationsElement>();
}

FPCGContext* FPCGExConsolidateRelationsElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExConsolidateRelationsContext* Context = new FPCGExConsolidateRelationsContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

void FPCGExConsolidateRelationsElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExRelationsProcessorElement::InitializeContext(InContext, InputData, SourceComponent, Node);
	//FPCGExConsolidateRelationsContext* Context = static_cast<FPCGExConsolidateRelationsContext*>(InContext);
	// ...
}

bool FPCGExConsolidateRelationsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExConsolidateRelationsElement::Execute);

	FPCGExConsolidateRelationsContext* Context = static_cast<FPCGExConsolidateRelationsContext*>(InContext);

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

		Context->SetState(PCGExMT::EState::ReadyForNextParams);

		// For each param, loop over points twice.
		// Params
		//		Points -> Capture delta
		//		Points -> Update data
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextParams))
	{
		if (!Context->AdvanceParams(true))
		{
			Context->SetState(PCGExMT::EState::Done); //No more params
		}
		else
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
		}
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO(false))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextParams); //No more points, move to next params
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingPoints);
		}
	}

	// 1st Pass on points

	auto InitializePointsInput = [&Context](UPCGExPointIO* IO)
	{
		Context->Deltas.Empty();
		IO->BuildMetadataEntries();
		Context->CurrentParams->PrepareForPointData(Context, IO->In); // Prepare to read IO->In
	};

	auto CapturePointDelta = [&Context](
		const FPCGPoint& Point, int32 ReadIndex, UPCGExPointIO* IO)
	{
		FWriteScopeLock ScopeLock(Context->DeltaLock);
		Context->Deltas.Add(Context->CachedIndex->GetValueFromItemKey(Point.MetadataEntry), ReadIndex); // Cache previous
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->CurrentIO->InputParallelProcessing(Context, InitializePointsInput, CapturePointDelta, 256))
		{
			Context->SetState(PCGExMT::EState::ProcessingPoints2ndPass);
		}
	}

	// 2nd Pass on points

	auto InitializePointsOutput = [&Context](UPCGExPointIO* IO)
	{
		Context->CurrentParams->PrepareForPointData(Context, IO->Out);
	};

	auto ConsolidatePoint = [&Context](
		const FPCGPoint& Point, int32 ReadIndex, UPCGExPointIO* IO)
	{
		const int64 CachedIndex =Context->CachedIndex->GetValueFromItemKey(Point.MetadataEntry); 
		Context->CachedIndex->SetValue(Point.MetadataEntry, ReadIndex);
		
		FReadScopeLock ScopeLock(Context->DeltaLock);

		for (PCGExRelational::FSocketInfos& SocketInfos : Context->SocketInfos)
		{
			const int64 RelationIndex = SocketInfos.Socket->GetRelationIndex(Point.MetadataEntry);

			if (RelationIndex == -1) { continue; } // No need to fix further

			const int64 FixedRelationIndex = GetFixedIndex(Context, RelationIndex);
			SocketInfos.Socket->SetRelationIndex(Point.MetadataEntry, FixedRelationIndex);

			EPCGExRelationType Type = EPCGExRelationType::Unknown;
			
			if (FixedRelationIndex != -1)
			{
				const int32 Key = IO->Out->GetPoint(FixedRelationIndex).MetadataEntry;
				for (PCGExRelational::FSocketInfos& OtherSocketInfos : Context->SocketInfos)
				{
					if (OtherSocketInfos.Socket->GetRelationIndex(Key) == CachedIndex)
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

	if (Context->IsState(PCGExMT::EState::ProcessingPoints2ndPass))
	{
		if (Context->CurrentIO->OutputParallelProcessing(Context, InitializePointsOutput, ConsolidatePoint, 256))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
		}
	}

	// Done

	if (Context->IsState(PCGExMT::EState::Done))
	{
		Context->Deltas.Empty();
		Context->Points->OutputTo(Context);
		Context->Params.OutputTo(Context);
		return true;
	}

	return false;
}

int64 FPCGExConsolidateRelationsElement::GetFixedIndex(FPCGExConsolidateRelationsContext* Context, int64 InIndex)
{
	if (const int64* FixedRelationIndexPtr = Context->Deltas.Find(InIndex)) { return *FixedRelationIndexPtr; }
	return -1;
}

#undef LOCTEXT_NAMESPACE
