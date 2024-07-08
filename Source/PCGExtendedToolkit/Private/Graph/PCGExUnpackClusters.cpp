// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExUnpackClusters.h"

#define LOCTEXT_NAMESPACE "PCGExUnpackClusters"
#define PCGEX_NAMESPACE UnpackClusters

PCGExData::EInit UPCGExUnpackClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

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

FPCGExUnpackClustersContext::~FPCGExUnpackClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(OutPoints)
	PCGEX_DELETE(OutEdges)
}


bool FPCGExUnpackClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(UnpackClusters)

	Context->OutPoints = new PCGExData::FPointIOCollection();
	Context->OutPoints->DefaultOutputLabel = PCGExGraph::OutputVerticesLabel;

	Context->OutEdges = new PCGExData::FPointIOCollection();
	Context->OutEdges->DefaultOutputLabel = PCGExGraph::OutputEdgesLabel;

	return true;
}

bool FPCGExUnpackClustersElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExUnpackClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(UnpackClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		while (Context->AdvancePointsIO(false)) { Context->GetAsyncManager()->Start<FPCGExUnpackClusterTask>(-1, Context->CurrentIO); }
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_ASYNC_WAIT

		Context->OutPoints->OutputTo(Context);
		Context->OutEdges->OutputTo(Context);

		Context->Done();
	}

	return Context->TryComplete();
}

bool FPCGExUnpackClusterTask::ExecuteTask()
{
	const FPCGExUnpackClustersContext* Context = Manager->GetContext<FPCGExUnpackClustersContext>();
	PCGEX_SETTINGS(UnpackClusters)

	const FPCGMetadataAttribute<int32>* EdgeCount = PointIO->GetIn()->Metadata->GetConstTypedAttribute<int32>(PCGExGraph::Tag_PackedClusterEdgeCount);
	if (!EdgeCount)
	{
		PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some input points have no packing metadata."));
		return false;
	}

	const int32 NumEdges = EdgeCount->GetValueFromItemKey(PCGInvalidEntryKey);
	const int32 NumVtx = PointIO->GetNum() - NumEdges;

	if (NumEdges > PointIO->GetNum() || NumVtx <= 0)
	{
		PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some input points have could not be unpacked correctly (wrong number of vtx or edges)."));
		return false;
	}

	const TArray<FPCGPoint>& PackedPoints = PointIO->GetIn()->GetPoints();

	PCGExData::FPointIO* NewEdges = Context->OutEdges->Emplace_GetRef(PointIO, PCGExData::EInit::NewOutput);
	TArray<FPCGPoint> MutableEdgePoints = NewEdges->GetOut()->GetMutablePoints();
	MutableEdgePoints.Append(&PackedPoints[0], NumEdges);
	NewEdges->GetOut()->SetPoints(MutableEdgePoints);

	UPCGMetadata* Metadata = NewEdges->GetOut()->Metadata;
	Metadata->DeleteAttribute(PCGExGraph::Tag_PackedClusterEdgeCount);
	Metadata->DeleteAttribute(PCGExGraph::Tag_PackedClusterPointCount);
	Metadata->DeleteAttribute(PCGExGraph::Tag_VtxEndpoint);

	PCGExData::FPointIO* NewVtx = Context->OutPoints->Emplace_GetRef(PointIO, PCGExData::EInit::NewOutput);
	TArray<FPCGPoint> MutableVtxPoints = NewVtx->GetOut()->GetMutablePoints();
	MutableVtxPoints.Append(&PackedPoints[NumEdges], NumVtx);
	NewVtx->GetOut()->SetPoints(MutableVtxPoints);

	Metadata = NewVtx->GetOut()->Metadata;
	Metadata->DeleteAttribute(PCGExGraph::Tag_PackedClusterEdgeCount);
	Metadata->DeleteAttribute(PCGExGraph::Tag_PackedClusterPointCount);
	Metadata->DeleteAttribute(PCGExGraph::Tag_EdgeEndpoints);

	FString OutPairId;
	PointIO->Tags->GetValue(PCGExGraph::TagStr_ClusterPair, OutPairId);

	PCGExGraph::MarkClusterVtx(NewVtx, OutPairId);
	PCGExGraph::MarkClusterEdges(NewEdges, OutPairId);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
