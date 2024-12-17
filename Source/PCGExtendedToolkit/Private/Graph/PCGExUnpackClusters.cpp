// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExUnpackClusters.h"


#define LOCTEXT_NAMESPACE "PCGExUnpackClusters"
#define PCGEX_NAMESPACE UnpackClusters

PCGExData::EIOInit UPCGExUnpackClustersSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

TArray<FPCGPinProperties> UPCGExUnpackClustersSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::SourcePackedClustersLabel, "Packed clusters", Required, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExUnpackClustersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Edges associated with the main output points", Required, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(UnpackClusters)

bool FPCGExUnpackClustersElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(UnpackClusters)

	Context->OutPoints = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->OutPoints->OutputPin = PCGExGraph::OutputVerticesLabel;

	Context->OutEdges = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->OutEdges->OutputPin = PCGExGraph::OutputEdgesLabel;

	return true;
}

bool FPCGExUnpackClustersElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExUnpackClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(UnpackClusters)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		TSharedPtr<PCGExMT::FTaskManager> AsyncManager = Context->GetAsyncManager();
		while (Context->AdvancePointsIO(false)) { PCGEX_LAUNCH(FPCGExUnpackClusterTask, Context->CurrentIO) }
		Context->SetAsyncState(PCGEx::State_WaitingOnAsyncWork);
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGEx::State_WaitingOnAsyncWork)
	{
		Context->OutPoints->StageOutputs();
		Context->OutEdges->StageOutputs();

		Context->Done();
	}

	return Context->TryComplete();
}

void FPCGExUnpackClusterTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
{
	const FPCGExUnpackClustersContext* Context = AsyncManager->GetContext<FPCGExUnpackClustersContext>();
	PCGEX_SETTINGS(UnpackClusters)

	const FPCGMetadataAttribute<int32>* EdgeCount = PointIO->GetIn()->Metadata->GetConstTypedAttribute<int32>(PCGExGraph::Tag_PackedClusterEdgeCount);
	if (!EdgeCount)
	{
		PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some input points have no packing metadata."));
		return;
	}

	const int32 NumEdges = EdgeCount->GetValue(PCGDefaultValueKey);
	const int32 NumVtx = PointIO->GetNum() - NumEdges;

	if (NumEdges > PointIO->GetNum() || NumVtx <= 0)
	{
		PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some input points have could not be unpacked correctly (wrong number of vtx or edges)."));
		return;
	}

	const TArray<FPCGPoint>& PackedPoints = PointIO->GetIn()->GetPoints();

	const TSharedPtr<PCGExData::FPointIO> NewEdges = Context->OutEdges->Emplace_GetRef(PointIO, PCGExData::EIOInit::New);
	TArray<FPCGPoint> MutableEdgePoints = NewEdges->GetOut()->GetMutablePoints();
	MutableEdgePoints.Append(&PackedPoints[0], NumEdges);
	NewEdges->GetOut()->SetPoints(MutableEdgePoints);

	NewEdges->DeleteAttribute(PCGExGraph::Tag_PackedClusterEdgeCount);
	NewEdges->DeleteAttribute(PCGExGraph::Tag_PackedClusterPointCount);
	NewEdges->DeleteAttribute(PCGExGraph::Tag_VtxEndpoint);

	const TSharedPtr<PCGExData::FPointIO> NewVtx = Context->OutPoints->Emplace_GetRef(PointIO, PCGExData::EIOInit::New);
	TArray<FPCGPoint> MutableVtxPoints = NewVtx->GetOut()->GetMutablePoints();
	MutableVtxPoints.Append(&PackedPoints[NumEdges], NumVtx);
	NewVtx->GetOut()->SetPoints(MutableVtxPoints);

	NewVtx->DeleteAttribute(PCGExGraph::Tag_PackedClusterEdgeCount);
	NewVtx->DeleteAttribute(PCGExGraph::Tag_PackedClusterPointCount);
	NewVtx->DeleteAttribute(PCGExGraph::Tag_EdgeEndpoints);

	FString OutPairId;
	PointIO->Tags->GetValue(PCGExGraph::TagStr_ClusterPair, OutPairId);

	PCGExGraph::MarkClusterVtx(NewVtx, OutPairId);
	PCGExGraph::MarkClusterEdges(NewEdges, OutPairId);
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
