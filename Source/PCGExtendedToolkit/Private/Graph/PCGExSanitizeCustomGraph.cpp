// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "..\..\Public\Graph\PCGExSanitizeCustomGraph.h"
#define PCGEX_NAMESPACE ConsolidateCustomGraph

#define LOCTEXT_NAMESPACE "PCGExConsolidateCustomGraph"

int32 UPCGExConsolidateCustomGraphSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_L; }

PCGExData::EInit UPCGExConsolidateCustomGraphSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(ConsolidateCustomGraph)

bool FPCGExConsolidateCustomGraphElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExCustomGraphProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ConsolidateCustomGraph)

	return true;
}

bool FPCGExConsolidateCustomGraphElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExConsolidateCustomGraphElement::Execute);

	PCGEX_CONTEXT(ConsolidateCustomGraph)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExGraph::State_ReadyForNextGraph);
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph(true)) { Context->Done(); }
		else { Context->SetState(PCGExMT::State_ReadyForNextPoints); }
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->SetState(PCGExGraph::State_ReadyForNextGraph); //No more points, move to next params
		}
		else
		{
			Context->IndicesRemap.Empty(Context->CurrentIO->GetNum());
			Context->CurrentIO->CreateOutKeys();

			if (!Context->PrepareCurrentGraphForPoints(*Context->CurrentIO, false))
			{
				PCGEX_GRAPH_MISSING_METADATA
				return false;
			}

			Context->SetState(PCGExGraph::State_CachingGraphIndices);
		}
	}

	// 1st Pass on points

	if (Context->IsState(PCGExGraph::State_CachingGraphIndices))
	{
		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			FWriteScopeLock WriteLock(Context->IndicesLock);
			const int32 CachedIndex = Context->GetCachedIndex(PointIndex);
			Context->IndicesRemap.Add(CachedIndex, PointIndex); // Store previous
			Context->SetCachedIndex(PointIndex, PointIndex);    // Update cached value with fresh one
		};

		if (!Context->ProcessCurrentPoints(ProcessPoint)) { return false; }

		Context->SetState(PCGExGraph::State_SwappingGraphIndices);
	}

	// 2nd Pass on points - Swap indices with updated ones
	if (Context->IsState(PCGExGraph::State_SwappingGraphIndices))
	{
		auto ConsolidatePoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			FReadScopeLock ReadLock(Context->IndicesLock);
			for (const PCGExGraph::FSocketInfos& SocketInfos : Context->SocketInfos)
			{
				const int32 OldRelationIndex = SocketInfos.Socket->GetTargetIndex(PointIndex);

				if (OldRelationIndex == -1) { continue; } // No need to fix further

				const int32 NewRelationIndex = GetFixedIndex(Context, OldRelationIndex);

				if (NewRelationIndex == -1) { SocketInfos.Socket->SetEdgeType(PointIndex, EPCGExEdgeType::Unknown); }

				SocketInfos.Socket->SetTargetIndex(PointIndex, NewRelationIndex);
			}
		};

		if (!Context->ProcessCurrentPoints(ConsolidatePoint)) { return false; }
		Context->SetState(PCGExGraph::State_FindingEdgeTypes);
	}

	// Optional 3rd Pass on points - Recompute edges type

	if (Context->IsState(PCGExGraph::State_FindingEdgeTypes))
	{
		auto ConsolidateEdgesType = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			ComputeEdgeType(Context->SocketInfos, PointIndex);
		};

		if (!Context->ProcessCurrentPoints(ConsolidateEdgesType)) { return false; }

		Context->WriteSocketInfos();
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	// Done

	if (Context->IsDone())
	{
		Context->IndicesRemap.Empty();
		Context->OutputPointsAndGraphParams();
	}

	return Context->IsDone();
}

int64 FPCGExConsolidateCustomGraphElement::GetFixedIndex(FPCGExConsolidateCustomGraphContext* Context, const int64 InIndex)
{
	if (const int64* FixedRelationIndexPtr = Context->IndicesRemap.Find(InIndex)) { return *FixedRelationIndexPtr; }
	return -1;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
