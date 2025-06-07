// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExUnpackClusters.h"


#define LOCTEXT_NAMESPACE "PCGExUnpackClusters"
#define PCGEX_NAMESPACE UnpackClusters

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

	FPCGAttributeIdentifier EdgeCountIdentifier = PCGEx::GetAttributeIdentifier(PCGExGraph::Tag_PackedClusterEdgeCount, PointIO->GetIn());
	const FPCGMetadataAttribute<int32>* EdgeCount = PointIO->GetIn()->Metadata->GetConstTypedAttribute<int32>(EdgeCountIdentifier);
	int32 NumEdges = -1;

	if (!EdgeCount)
	{
		// Support for legacy data that was storing the edge count as a point index
		EdgeCountIdentifier = PCGEx::GetAttributeIdentifier(PCGExGraph::Tag_PackedClusterEdgeCount_LEGACY, PointIO->GetIn());
		EdgeCount = PointIO->GetIn()->Metadata->GetConstTypedAttribute<int32>(EdgeCountIdentifier);
		if (EdgeCount) { NumEdges = EdgeCount->GetValue(PCGDefaultValueKey); }
	}
	else
	{
		NumEdges = EdgeCount->GetValue(PCGFirstEntryKey);
	}

	if (NumEdges == -1)
	{
		PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some input points have no packing metadata."));
		return;
	}

	const int32 NumVtx = PointIO->GetNum() - NumEdges;

	if (NumEdges > PointIO->GetNum() || NumVtx <= 0)
	{
		PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some input points have could not be unpacked correctly (wrong number of vtx or edges)."));
		return;
	}

	const UPCGBasePointData* PackedPoints = PointIO->GetIn();
	EPCGPointNativeProperties AllocateProperties = PackedPoints->GetAllocatedProperties();

	const TSharedPtr<PCGExData::FPointIO> NewEdges = Context->OutEdges->Emplace_GetRef(PointIO, PCGExData::EIOInit::New);
	UPCGBasePointData* MutableEdgePoints = NewEdges->GetOut();
	PCGEx::SetNumPointsAllocated(MutableEdgePoints, NumEdges, AllocateProperties);
	NewEdges->InheritPoints(0, 0, NumEdges);

	NewEdges->DeleteAttribute(EdgeCountIdentifier);
	NewEdges->DeleteAttribute(PCGExGraph::Attr_PCGExVtxIdx);

	const TSharedPtr<PCGExData::FPointIO> NewVtx = Context->OutPoints->Emplace_GetRef(PointIO, PCGExData::EIOInit::New);
	UPCGBasePointData* MutableVtxPoints = NewVtx->GetOut();
	PCGEx::SetNumPointsAllocated(MutableVtxPoints, NumVtx, AllocateProperties);
	NewVtx->InheritPoints(NumEdges, 0, NumVtx);

	NewVtx->DeleteAttribute(EdgeCountIdentifier);
	NewVtx->DeleteAttribute(PCGExGraph::Attr_PCGExEdgeIdx);

	const PCGExTags::IDType PairId = PointIO->Tags->GetTypedValue<int32>(PCGExGraph::TagStr_PCGExCluster);

	PCGExGraph::MarkClusterVtx(NewVtx, PairId);
	PCGExGraph::MarkClusterEdges(NewEdges, PairId);
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
