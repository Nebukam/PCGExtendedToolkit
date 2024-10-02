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

bool FPCGExPackClustersElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PackClusters)

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

	Context->PackedClusters = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->PackedClusters->DefaultOutputLabel = PCGExGraph::OutputPackedClustersLabel;

	return true;
}

bool FPCGExPackClustersElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPackClustersElement::Execute);

	PCGEX_CONTEXT(PackClusters)
	PCGEX_EXECUTION_CHECK

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

			for (TSharedPtr<PCGExData::FPointIO> EdgeIO : Context->TaggedEdges->Entries)
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
		Context->PackedClusters->OutputToContext();
	}

	return Context->TryComplete();
}

bool FPCGExPackClusterTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
{
	FPCGExPackClustersContext* Context = AsyncManager->GetContext<FPCGExPackClustersContext>();
	PCGEX_SETTINGS(PackClusters)

	const TSharedPtr<PCGEx::FAttributesInfos> VtxAttributes = PCGEx::FAttributesInfos::Get(PointIO->GetIn()->Metadata);

	const TSharedPtr<PCGExData::FPointIO> PackedIO = Context->PackedClusters->Emplace_GetRef(InEdges);
	PackedIO->InitializeOutput<UPCGPointData>(Context, PCGExData::EInit::DuplicateInput);

	int32 NumEdges = 0;
	TArray<int32> ReducedVtxIndices;

	if (!PCGExGraph::GetReducedVtxIndices(InEdges, &EndpointsLookup, ReducedVtxIndices, NumEdges)) { return false; }

	TArray<FPCGPoint>& MutablePoints = PackedIO->GetOut()->GetMutablePoints();
	MutablePoints.SetNum(NumEdges + ReducedVtxIndices.Num());

	PackedIO->CleanupKeys();

	const TArrayView<const int32> View = MakeArrayView(ReducedVtxIndices);
	PCGEx::CopyPoints(PointIO.Get(), PackedIO.Get(), View, NumEdges);

	for (const PCGEx::FAttributeIdentity& Identity : VtxAttributes->Identities)
	{
		CopyValues(AsyncManager, Identity, PointIO, PackedIO, View, NumEdges);
	}

	WriteMark(PackedIO.ToSharedRef(), PCGExGraph::Tag_PackedClusterEdgeCount, NumEdges);

	PCGExGraph::CleanupClusterTags(PackedIO);

	FString OutPairId;
	PackedIO->Tags->Add(PCGExGraph::TagStr_ClusterPair, InEdges->GetIn()->UID, OutPairId);

	InEdges->CleanupKeys();

	Context->CarryOverDetails.Filter(PointIO->Tags.Get());
	Context->CarryOverDetails.Filter(InEdges->Tags.Get());

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
