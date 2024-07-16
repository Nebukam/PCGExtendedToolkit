// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExPackClusters.h"

#include "Data/PCGExPointIOMerger.h"
#include "Geometry/PCGExGeoDelaunay.h"

#define LOCTEXT_NAMESPACE "PCGExPackClusters"
#define PCGEX_NAMESPACE PackClusters

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

	PackedClusterList.Empty();
	PCGEX_DELETE(PackedClusters)
}


bool FPCGExPackClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PackClusters)

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

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
		int32 IOIndex = 0;
		while (Context->AdvancePointsIO(false))
		{
			if (!Context->TaggedEdges) { continue; }

			Context->PackedClusterList.SetNum(Context->PackedClusterList.Num() + Context->TaggedEdges->Entries.Num());

			for (PCGExData::FPointIO* EdgeIO : Context->TaggedEdges->Entries)
			{
				Context->GetAsyncManager()->Start<FPCGExPackClusterTask>(IOIndex++, Context->CurrentIO, EdgeIO, Context->EndpointsLookup);
			}
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_ASYNC_WAIT
		Context->Done();
	}

	if (Context->IsDone())
	{
		Context->PackedClusters->AddUnsafe(Context->PackedClusterList);
		Context->PackedClusters->OutputTo(Context);
	}

	return Context->TryComplete();
}

bool FPCGExPackClusterTask::ExecuteTask()
{
	FPCGExPackClustersContext* Context = Manager->GetContext<FPCGExPackClustersContext>();
	PCGEX_SETTINGS(PackClusters)

	PCGEx::FAttributesInfos* VtxAttributes = PCGEx::FAttributesInfos::Get(PointIO->GetIn()->Metadata);

	InEdges->CreateInKeys();
	PCGExData::FPointIO* PackedIO = new PCGExData::FPointIO(InEdges);
	PackedIO->InitializeOutput<UPCGPointData>(PCGExData::EInit::DuplicateInput);
	Context->PackedClusterList[TaskIndex] = PackedIO;

	int32 NumEdges = 0;
	TArray<int32> ReducedVtxIndices;

	if (!PCGExGraph::GetReducedVtxIndices(InEdges, &EndpointsLookup, ReducedVtxIndices, NumEdges)) { return false; }

	TArray<FPCGPoint>& MutablePoints = PackedIO->GetOut()->GetMutablePoints();
	MutablePoints.SetNum(NumEdges + ReducedVtxIndices.Num());

	PackedIO->CleanupKeys();
	PackedIO->CreateOutKeys();

	const TArrayView<const int32> View = MakeArrayView(ReducedVtxIndices);
	PCGEx::CopyPoints(PointIO, PackedIO, View, NumEdges);

	for (const PCGEx::FAttributeIdentity& Identity : VtxAttributes->Identities)
	{
		CopyValues(Manager, Identity, PointIO, PackedIO, View, NumEdges);
	}

	WriteMark(PackedIO, PCGExGraph::Tag_PackedClusterEdgeCount, NumEdges);

	PCGExGraph::CleanupClusterTags(PackedIO);

	FString OutPairId;
	PackedIO->Tags->Set(PCGExGraph::TagStr_ClusterPair, InEdges->GetIn()->UID, OutPairId);

	InEdges->CleanupKeys();

	Context->CarryOverDetails.Filter(PointIO->Tags);
	Context->CarryOverDetails.Filter(InEdges->Tags);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
