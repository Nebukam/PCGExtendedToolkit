// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExUnpackClusters.h"

#define LOCTEXT_NAMESPACE "PCGExUnpackClusters"
#define PCGEX_NAMESPACE UnpackClusters

UPCGExUnpackClustersSettings::UPCGExUnpackClustersSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EInit UPCGExUnpackClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FName UPCGExUnpackClustersSettings::GetMainInputLabel() const { return PCGExGraph::SourcePackedClustersLabel; }

FName UPCGExUnpackClustersSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

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
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

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
		while (Context->AdvancePointsIO()) { Context->GetAsyncManager()->Start<FPCGExUnpackClusterTask>(-1, Context->CurrentIO); }
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC

		Context->OutPoints->OutputTo(Context);
		Context->OutEdges->OutputTo(Context);

		Context->Done();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

bool FPCGExUnpackClusterTask::ExecuteTask()
{
	const FPCGExUnpackClustersContext* Context = Manager->GetContext<FPCGExUnpackClustersContext>();
	PCGEX_SETTINGS(UnpackClusters)

	PCGExData::FPointIO& PackedIO = *PointIO;

	const FPCGMetadataAttribute<int32>* EdgeCount = PackedIO.GetIn()->Metadata->GetConstTypedAttribute<int32>(PCGExGraph::Tag_PackedClusterEdgeCount);
	if (!EdgeCount)
	{
		PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some input points have no packing metadata."));
		return false;
	}

	const int32 NumEdges = EdgeCount->GetValueFromItemKey(PCGInvalidEntryKey);
	const int32 NumVtx = PackedIO.GetNum() - NumEdges;

	if (NumEdges > PackedIO.GetNum() || NumVtx <= 0)
	{
		PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some input points have could not be unpacked correctly (wrong number of vtx or edges)."));
		return false;
	}

	PCGExData::FPointIO& NewEdges = Context->OutEdges->Emplace_GetRef(PackedIO, PCGExData::EInit::DuplicateInput);
	TArray<FPCGPoint> MutableEdgePoints = NewEdges.GetOut()->GetMutablePoints();
	MutableEdgePoints.Append(MakeArrayView(PackedIO.GetIn()->GetPoints().GetData(), NumEdges));

	UPCGMetadata* Metadata = NewEdges.GetOut()->Metadata;
	Metadata->DeleteAttribute(PCGExGraph::Tag_PackedClusterEdgeCount);
	Metadata->DeleteAttribute(PCGExGraph::Tag_PackedClusterPointCount);
	Metadata->DeleteAttribute(PCGExGraph::Tag_EdgesNum);
	Metadata->DeleteAttribute(PCGExGraph::Tag_EdgeIndex);

	PCGExData::FPointIO& NewVtx = Context->OutPoints->Emplace_GetRef(PackedIO, PCGExData::EInit::DuplicateInput);
	TArray<FPCGPoint> MutableVtxPoints = NewVtx.GetOut()->GetMutablePoints();
	MutableVtxPoints.Append(MakeArrayView(PackedIO.GetIn()->GetPoints().GetData() + NumEdges, NumVtx));

	Metadata = NewVtx.GetOut()->Metadata;
	Metadata->DeleteAttribute(PCGExGraph::Tag_PackedClusterEdgeCount);
	Metadata->DeleteAttribute(PCGExGraph::Tag_PackedClusterPointCount);
	Metadata->DeleteAttribute(PCGExGraph::Tag_EdgeEndpoints);

	FString OutPairId;
	PackedIO.Tags->GetValue(PCGExGraph::TagStr_ClusterPair, OutPairId);
	NewEdges.Tags->Set(PCGExGraph::TagStr_ClusterPair, OutPairId);
	NewVtx.Tags->Set(PCGExGraph::TagStr_ClusterPair, OutPairId);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
