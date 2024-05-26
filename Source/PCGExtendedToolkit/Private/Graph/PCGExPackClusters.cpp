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
	FPCGPinProperties& PinEdgesOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputPackedClustersLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinEdgesOutput.Tooltip = FTEXT("Individually packed clusters");
#endif

	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(PackClusters)


FPCGExPackClustersContext::~FPCGExPackClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(PackedClusters)
	PCGEX_DELETE(VtxAttributes)
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
		PCGEX_DELETE(Context->VtxAttributes)

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no associated edges."));
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
				return false;
			}

			Context->VtxAttributes = PCGEx::FAttributesInfos::Get(Context->CurrentIO->GetIn()->Metadata);

			for (PCGExData::FPointIO* EdgeIO : Context->TaggedEdges->Entries)
			{
				EdgeIO->CreateInKeys();
				PCGExData::FPointIO& PackedIO = Context->PackedClusters->Emplace_GetRef(*EdgeIO, PCGExData::EInit::DuplicateInput); // Pack from edges
				Context->GetAsyncManager()->Start<FPCGExPackClusterTask>(PackedIO.IOIndex, &PackedIO, Context->CurrentIO, EdgeIO);
			}

			Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
		}
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->PackedClusters->OutputTo(Context);
	}

	return Context->IsDone();
}

bool FPCGExPackClusterTask::ExecuteTask()
{
	const FPCGExPackClustersContext* Context = Manager->GetContext<FPCGExPackClustersContext>();

	int32 NumEdges = 0;
	TArray<int32> ReducedVtxIndices;

	if (!PCGExGraph::GetReducedVtxIndices(*InEdges, &Context->NodeIndicesMap, ReducedVtxIndices, NumEdges)) { return false; }

	TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
	MutablePoints.SetNum(NumEdges + ReducedVtxIndices.Num());

	PointIO->CleanupKeys();
	PointIO->CreateOutKeys();

	const TArrayView<int32> View = MakeArrayView(ReducedVtxIndices);
	PCGEx::CopyPoints(*InVtx, *PointIO, View, NumEdges);

	for (const PCGEx::FAttributeIdentity& Identity : Context->VtxAttributes->Identities)
	{
		CopyValues(Identity, *InVtx, *PointIO, View, NumEdges);
	}

	WriteMark(*PointIO, PCGExGraph::Tag_PackedClusterEdgeCount, NumEdges);

	FString OutPairId;
	PointIO->Tags->Set(PCGExGraph::TagStr_ClusterPair, InEdges->GetIn()->UID, OutPairId);
	PointIO->Flatten();

	InEdges->CleanupKeys();

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
