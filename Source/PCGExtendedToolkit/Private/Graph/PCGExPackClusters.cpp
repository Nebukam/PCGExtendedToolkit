// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExPackClusters.h"
#include "Data/PCGExPointIOMerger.h"

#include "Geometry/PCGExGeoDelaunay.h"

#define LOCTEXT_NAMESPACE "PCGExPackClusters"
#define PCGEX_NAMESPACE PackClusters

PCGExData::EIOInit UPCGExPackClustersSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

PCGExData::EIOInit UPCGExPackClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::None; }

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
	Context->PackedClusters->OutputPin = PCGExGraph::OutputPackedClustersLabel;

	return true;
}

bool FPCGExPackClustersElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPackClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PackClusters)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatch<PCGExPackClusters::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::TBatch<PCGExPackClusters::FProcessor>>& NewBatch)
			{
			}))
		{
			Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)
	Context->PackedClusters->StageOutputs();
	return Context->TryComplete();
}

namespace PCGExPackClusters
{
	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		if (!TProcessor::Process(InAsyncManager)) { return false; }

		PCGEx::InitArray(ReducedVtxIndex, NumNodes);
		for (int i = 0; i < NumNodes; i++) { *(ReducedVtxIndex->GetData() + i) = Cluster->GetNodePointIndex(i); }

		PackedIO = Context->PackedClusters->Emplace_GetRef(EdgeDataFacade->Source, PCGExData::EIOInit::Duplicate);
		PackedIOFacade = MakeShared<PCGExData::FFacade>(PackedIO.ToSharedRef());


		VtxStartIndex = PackedIO->GetNum();
		NumIndices = ReducedVtxIndex->Num();

		if (VtxStartIndex <= 0) { return false; }
		if (NumIndices <= 0) { return false; }

		PackedIO->Tags->Set<int32>(PCGExGraph::TagStr_PCGExCluster, EdgeDataFacade->GetIn()->GetUniqueID());
		WriteMark(PackedIO.ToSharedRef(), PCGExGraph::Tag_PackedClusterEdgeCount, NumEdges);

		// Copy vtx points after edge points
		const TArray<FPCGPoint>& VtxPoints = VtxDataFacade->GetIn()->GetPoints();
		TArray<FPCGPoint>& PackedPoints = PackedIO->GetMutablePoints();

		PackedPoints.SetNumUninitialized(PackedPoints.Num() + NumIndices);

		for (int i = 0; i < NumIndices; i++)
		{
			PackedPoints[VtxStartIndex + i] = VtxPoints[*(ReducedVtxIndex->GetData() + i)];
			PackedPoints[VtxStartIndex + i].MetadataEntry = PCGInvalidEntryKey;
		}

		//
		VtxAttributes = PCGEx::FAttributesInfos::Get(VtxDataFacade->GetIn()->Metadata);
		if (VtxAttributes->Identities.IsEmpty()) { return true; }

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, CopyVtxAttributes)

		CopyVtxAttributes->OnIterationCallback = [PCGEX_ASYNC_THIS_CAPTURE]
			(const int32 Index, const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS

				const PCGEx::FAttributeIdentity& Identity = This->VtxAttributes->Identities[Index];

				PCGEx::ExecuteWithRightType(
					Identity.UnderlyingType, [&](auto DummyValue)
					{
						using T = decltype(DummyValue);
						TArray<T> RawValues;

						TSharedPtr<PCGExData::TBuffer<T>> InValues = This->VtxDataFacade->GetReadable<T>(Identity.Name);
						TSharedPtr<PCGExData::TBuffer<T>> OutValues = This->PackedIOFacade->GetWritable<T>(InValues->GetTypedInAttribute(), PCGExData::EBufferInit::New);

						const TArray<int32>& ReducedVtxIndices = *This->ReducedVtxIndex;
						for (int i = 0; i < ReducedVtxIndices.Num(); i++) { OutValues->GetMutable(This->VtxStartIndex + i) = InValues->Read(ReducedVtxIndices[i]); }
					});
			};

		CopyVtxAttributes->StartIterations(VtxAttributes->Identities.Num(), 1, false);

		Context->CarryOverDetails.Prune(PackedIO->Tags.Get());

		return true;
	}

	void FProcessor::CompleteWork()
	{
		TProcessor<FPCGExPackClustersContext, UPCGExPackClustersSettings>::CompleteWork();
		PackedIOFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
