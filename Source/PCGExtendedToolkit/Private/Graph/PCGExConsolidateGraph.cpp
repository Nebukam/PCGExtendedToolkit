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

	const UPCGExConsolidateGraphSettings* Settings = Context->GetInputSettings<UPCGExConsolidateGraphSettings>();
	check(Settings);

	Context->bConsolidateEdgeType = Settings->bConsolidateEdgeType;
	return Context;
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

	auto InitializePointsFirstPass = [&Context](UPCGExPointIO* IO)
	{
		Context->IndicesRemap.Empty(IO->NumPoints);
		IO->BuildMetadataEntries();
		Context->CurrentGraph->PrepareForPointData(IO->Out, true); // Prepare to read IO->Out
	};

	auto ProcessPoint = [&Context](const FPCGPoint& Point, const int32 ReadIndex, const UPCGExPointIO* IO)
	{
		FWriteScopeLock ScopeLock(Context->IndicesLock);
		const int64 Key = Point.MetadataEntry;
		const int64 CachedIndex = Context->CachedIndex->GetValueFromItemKey(Key);
		Context->IndicesRemap.Add(CachedIndex, ReadIndex); // Store previous
		Context->CachedIndex->SetValue(Key, ReadIndex);    // Update cached value with fresh one
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->CurrentIO->OutputParallelProcessing(Context, InitializePointsFirstPass, ProcessPoint, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::ProcessingPoints2ndPass);
		}
	}

	// 2nd Pass on points - Swap indices with updated ones

	auto InitializeNone = [](const UPCGExPointIO* IO)
	{
		// Dummy lambda
	};

	auto ConsolidatePoint = [&Context](const FPCGPoint& Point, const int32 ReadIndex, const UPCGExPointIO* IO)
	{
		FReadScopeLock ScopeLock(Context->IndicesLock);

		for (const PCGExGraph::FSocketInfos& SocketInfos : Context->SocketInfos)
		{
			const int64 OldRelationIndex = SocketInfos.Socket->GetTargetIndex(Point.MetadataEntry);

			if (OldRelationIndex == -1) { continue; } // No need to fix further

			const int64 NewRelationIndex = GetFixedIndex(Context, OldRelationIndex);
			const PCGMetadataEntryKey Key = Point.MetadataEntry;
			PCGMetadataEntryKey NewEntryKey = PCGInvalidEntryKey;

			if (NewRelationIndex != -1) { NewEntryKey = IO->Out->GetPoint(NewRelationIndex).MetadataEntry; }
			else { SocketInfos.Socket->SetEdgeType(Key, EPCGExEdgeType::Unknown); }

			SocketInfos.Socket->SetTargetIndex(Key, NewRelationIndex);
			SocketInfos.Socket->SetTargetEntryKey(Key, NewEntryKey);
		}
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints2ndPass))
	{
		if (Context->CurrentIO->OutputParallelProcessing(Context, InitializeNone, ConsolidatePoint, Context->ChunkSize))
		{
			Context->SetState(Context->bConsolidateEdgeType ? PCGExMT::EState::ProcessingPoints3rdPass : PCGExMT::EState::ReadyForNextPoints);
		}
	}

	// Optional 3rd Pass on points - Recompute edges type

	auto ConsolidateEdgesType = [&Context](const FPCGPoint& Point, const int32 ReadIndex, const UPCGExPointIO* IO)
	{
		Context->ComputeEdgeType(Point, ReadIndex, IO);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints3rdPass))
	{
		if (Context->CurrentIO->OutputParallelProcessing(Context, InitializeNone, ConsolidateEdgesType, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
		}
	}

	// Done

	if (Context->IsState(PCGExMT::EState::Done))
	{
		Context->IndicesRemap.Empty();
		Context->OutputPointsAndParams();
		return true;
	}

	return false;
}

int64 FPCGExConsolidateGraphElement::GetFixedIndex(FPCGExConsolidateGraphContext* Context, int64 InIndex)
{
	if (const int64* FixedRelationIndexPtr = Context->IndicesRemap.Find(InIndex)) { return *FixedRelationIndexPtr; }
	return -1;
}

#undef LOCTEXT_NAMESPACE
