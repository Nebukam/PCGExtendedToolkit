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

	Context->VtxPartitions = new PCGExData::FPointIOGroup();
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

				FString ClusterID;
				PointPartitionIO.Tags->Set(PCGExGraph::Tag_Cluster, PointPartitionIO.GetOut()->UID, ClusterID);
				EdgeIO->Tags->Set(PCGExGraph::Tag_Cluster, ClusterID);

				EdgeIO->CreateInKeys();
				Context->GetAsyncManager()->Start<FPCGExCreateVtxPartitionTask>(i, &PointPartitionIO, EdgeIO, &Context->NodeIndicesMap);
			}

			Context->SetAsyncState(PCGExGraph::State_ProcessingEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->VtxPartitions->OutputTo(Context);
		Context->MainEdges->OutputTo(Context);
	}

	return Context->IsDone();
}

bool FPCGExCreateVtxPartitionTask::ExecuteTask()
{
	const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
	TArray<FPCGPoint>& OutPoints = PointIO->GetOut()->GetMutablePoints();

	PCGEx::TFAttributeReader<int32>* StartIndexReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgeStart);
	if (!StartIndexReader->Bind(*EdgeIO))
	{
		PCGEX_DELETE(StartIndexReader)
		return false;
	}

	PCGEx::TFAttributeReader<int32>* EndIndexReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgeEnd);
	if (!EndIndexReader->Bind(*EdgeIO))
	{
		PCGEX_DELETE(StartIndexReader)
		PCGEX_DELETE(EndIndexReader)
		return false;
	}

	TSet<int32> UniqueIndices;
	UniqueIndices.Reserve(StartIndexReader->Values.Num());

#define PCGEX_PUSH_VTX(_PTR)\
	if (!UniqueIndices.Contains(*_PTR))	{\
		OutPoints.Add(InPoints[*_PTR]);\
		UniqueIndices.Add(*_PTR);}

	for (int i = 0; i < EdgeIO->GetNum(); i++)
	{
		const int32* NodeStartPtr = NodeIndicesMap->Find(StartIndexReader->Values[i]);
		const int32* NodeEndPtr = NodeIndicesMap->Find(EndIndexReader->Values[i]);

		if ((!NodeStartPtr || !NodeEndPtr) ||
			(*NodeStartPtr == *NodeEndPtr)) { continue; }

		PCGEX_PUSH_VTX(NodeStartPtr)
		PCGEX_PUSH_VTX(NodeEndPtr)
	}

#undef PCGEX_PUSH_VTX

	UniqueIndices.Empty();

	PCGEX_DELETE(StartIndexReader)
	PCGEX_DELETE(EndIndexReader)

	return true;
}

#undef LOCTEXT_NAMESPACE
