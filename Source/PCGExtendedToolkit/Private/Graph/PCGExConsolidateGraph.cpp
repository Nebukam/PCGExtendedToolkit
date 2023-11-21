// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExConsolidateGraph.h"

#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Graph/PCGExGraphHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExConsolidateGraph"

int32 UPCGExConsolidateGraphSettings::GetPreferredChunkSize() const { return 32; }

PCGEx::EIOInit UPCGExConsolidateGraphSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

FPCGElementPtr UPCGExConsolidateGraphSettings::CreateElement() const
{
	return MakeShared<FPCGExConsolidateGraphElement>();
}

FPCGContext* FPCGExConsolidateGraphElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExConsolidateGraphContext* Context = new FPCGExConsolidateGraphContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

void FPCGExConsolidateGraphElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExGraphProcessorElement::InitializeContext(InContext, InputData, SourceComponent, Node);
	//FPCGExConsolidateGraphContext* Context = static_cast<FPCGExConsolidateGraphContext*>(InContext);
	// ...
}

bool FPCGExConsolidateGraphElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExConsolidateGraphElement::Execute);

	FPCGExConsolidateGraphContext* Context = static_cast<FPCGExConsolidateGraphContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
		Context->SetState(PCGExMT::EState::ReadyForNextGraph);
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph(true))
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
			Context->SetState(PCGExMT::EState::ReadyForNextGraph); //No more points, move to next params
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
		Context->CurrentGraph->PrepareForPointData(Context, IO->In, false); // Prepare to read IO->In
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

	auto InitializePointsOutput = [&Context](const UPCGExPointIO* IO)
	{
		Context->CurrentGraph->PrepareForPointData(Context, IO->Out, false);
	};

	auto ConsolidatePoint = [&Context](
		const FPCGPoint& Point, const int32 ReadIndex, const UPCGExPointIO* IO)
	{
		const int64 CachedIndex = Context->CachedIndex->GetValueFromItemKey(Point.MetadataEntry);
		Context->CachedIndex->SetValue(Point.MetadataEntry, ReadIndex);

		FReadScopeLock ScopeLock(Context->DeltaLock);

		// BUG: Under certain conditions, complete connections are broken and indices are not
		// consolidated properly.
		
		for (PCGExGraph::FSocketInfos& SocketInfos : Context->SocketInfos)
		{
			const int64 RelationIndex = SocketInfos.Socket->GetTargetIndex(Point.MetadataEntry);

			if (RelationIndex == -1) { continue; } // No need to fix further

			const int64 FixedRelationIndex = GetFixedIndex(Context, RelationIndex);
			SocketInfos.Socket->SetTargetIndex(Point.MetadataEntry, FixedRelationIndex);

			EPCGExEdgeType Type = EPCGExEdgeType::Unknown;

			if (FixedRelationIndex != -1)
			{
				const int32 Key = IO->Out->GetPoint(FixedRelationIndex).MetadataEntry;
				for (PCGExGraph::FSocketInfos& OtherSocketInfos : Context->SocketInfos)
				{
					if (OtherSocketInfos.Socket->GetTargetIndex(Key) == CachedIndex)
					{
						//TODO: Handle cases where there can be multiple sockets with a valid connection
						Type = PCGExGraph::Helpers::GetEdgeType(SocketInfos, OtherSocketInfos);
					}
				}

				if (Type == EPCGExEdgeType::Unknown) { Type = EPCGExEdgeType::Unique; }
			}
			else
			{
				SocketInfos.Socket->SetTargetEntryKey(Point.MetadataEntry, PCGInvalidEntryKey);
			}

			SocketInfos.Socket->SetEdgeType(Point.MetadataEntry, Type);
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
		Context->OutputPointsAndParams();
		return true;
	}

	return false;
}

int64 FPCGExConsolidateGraphElement::GetFixedIndex(FPCGExConsolidateGraphContext* Context, int64 InIndex)
{
	if (const int64* FixedRelationIndexPtr = Context->Deltas.Find(InIndex)) { return *FixedRelationIndexPtr; }
	return -1;
}

#undef LOCTEXT_NAMESPACE
