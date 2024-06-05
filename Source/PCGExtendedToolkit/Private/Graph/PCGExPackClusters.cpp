// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExPackClusters.h"

#include "Data/PCGExPointIOMerger.h"
#include "Geometry/PCGExGeoDelaunay.h"

#define LOCTEXT_NAMESPACE "PCGExPackClusters"
#define PCGEX_NAMESPACE PackClusters

UPCGExPackClustersSettings::UPCGExPackClustersSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EInit UPCGExPackClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGExData::EInit UPCGExPackClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

TArray<FPCGPinProperties> UPCGExPackClustersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputPackedClustersLabel, "Individually packed clusters", Required, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(PackClusters)


FPCGExPackClustersContext::~FPCGExPackClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(PackedClusters)
}


bool FPCGExPackClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PackClusters)

	Context->PackedClusters = new PCGExData::FPointIOCollection();
	Context->PackedClusters->DefaultOutputLabel = PCGExGraph::OutputPackedClustersLabel;

	return true;
}

bool FPCGExPackClustersElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPackClustersElement::Execute);

	PCGEX_CONTEXT(PackClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		while (Context->AdvancePointsIO())
		{
			if (!Context->TaggedEdges)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no associated edges."));
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
				return false;
			}

			for (PCGExData::FPointIO* EdgeIO : Context->TaggedEdges->Entries)
			{
				Context->GetAsyncManager()->Start<FPCGExPackClusterTask>(-1, Context->CurrentIO, EdgeIO, Context->NodeIndicesMap);
			}
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC
		Context->Done();
	}

	if (Context->IsDone())
	{
		Context->PackedClusters->OutputTo(Context);
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

bool FPCGExPackClusterTask::ExecuteTask()
{
	const FPCGExPackClustersContext* Context = Manager->GetContext<FPCGExPackClustersContext>();
	PCGEX_SETTINGS(PackClusters)

	PCGEx::FAttributesInfos* VtxAttributes = PCGEx::FAttributesInfos::Get(PointIO->GetIn()->Metadata);

	InEdges->CreateInKeys();
	PCGExData::FPointIO& PackedIO = Context->PackedClusters->Emplace_GetRef(*InEdges, PCGExData::EInit::DuplicateInput);

	int32 NumEdges = 0;
	TArray<int32> ReducedVtxIndices;

	if (!PCGExGraph::GetReducedVtxIndices(*InEdges, &NodeIndicesMap, ReducedVtxIndices, NumEdges)) { return false; }

	TArray<FPCGPoint>& MutablePoints = PackedIO.GetOut()->GetMutablePoints();
	MutablePoints.SetNum(NumEdges + ReducedVtxIndices.Num());

	PackedIO.CleanupKeys();
	PackedIO.CreateOutKeys();

	const TArrayView<int32> View = MakeArrayView(ReducedVtxIndices);
	PCGEx::CopyPoints(*PointIO, PackedIO, View, NumEdges);

	for (const PCGEx::FAttributeIdentity& Identity : VtxAttributes->Identities)
	{
		CopyValues(Identity, *PointIO, PackedIO, View, NumEdges);
	}

	WriteMark(PackedIO, PCGExGraph::Tag_PackedClusterEdgeCount, NumEdges);

	FString OutPairId;
	PackedIO.Tags->Set(PCGExGraph::TagStr_ClusterPair, InEdges->GetIn()->UID, OutPairId);

	InEdges->CleanupKeys();

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
