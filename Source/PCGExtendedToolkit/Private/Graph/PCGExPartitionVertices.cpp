// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExPartitionVertices.h"

#include "Data/PCGExGraphDefinition.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExPartitionVerticesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExPartitionVerticesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::Forward; }

#pragma endregion

FPCGExPartitionVerticesContext::~FPCGExPartitionVerticesContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(VtxPartitions)

	IndexedEdges.Empty();
}

PCGEX_INITIALIZE_ELEMENT(PartitionVertices)

bool FPCGExPartitionVerticesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PartitionVertices)

	Context->VtxPartitions = new PCGExData::FPointIOCollection();
	Context->VtxPartitions->DefaultOutputLabel = PCGExGraph::OutputVerticesLabel;

	return true;
}

bool FPCGExPartitionVerticesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPartitionVerticesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PartitionVertices)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges) { return false; }

			for (int i = 0; i < Context->TaggedEdges->Entries.Num(); i++)
			{
				PCGExData::FPointIO& PointPartitionIO = Context->VtxPartitions->Emplace_GetRef(*Context->CurrentIO, PCGExData::EInit::NewOutput);
				PCGExData::FPointIO* EdgeIO = Context->TaggedEdges->Entries[i];

				FString OutId;
				PointPartitionIO.Tags->Set(PCGExGraph::TagStr_ClusterPair, PointPartitionIO.GetOut()->UID, OutId);
				EdgeIO->Tags->Set(PCGExGraph::TagStr_ClusterPair, OutId);

				EdgeIO->CreateInKeys();
				Context->GetAsyncManager()->Start<FPCGExCreateVtxPartitionTask>(i, &PointPartitionIO, EdgeIO, &Context->EndpointsLookup);
			}

			Context->SetAsyncState(PCGExGraph::State_ProcessingEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		PCGEX_WAIT_ASYNC
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->VtxPartitions->OutputTo(Context);
		Context->MainEdges->OutputTo(Context);
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

bool FPCGExCreateVtxPartitionTask::ExecuteTask()
{
	TArray<int32> ReducedVtxIndices;

	int32 NumEdges = 0;
	if (!PCGExGraph::GetReducedVtxIndices(*EdgeIO, NodeIndicesMap, ReducedVtxIndices, NumEdges)) { return false; }

	const TArrayView<int32> View = MakeArrayView(ReducedVtxIndices);

	PointIO->GetOut()->GetMutablePoints().SetNum(View.Num());
	PCGEx::CopyPoints(*PointIO, *PointIO, View, 0, true);

	return true;
}

#undef LOCTEXT_NAMESPACE
