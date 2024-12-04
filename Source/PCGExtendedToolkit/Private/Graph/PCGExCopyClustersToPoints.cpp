// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCopyClustersToPoints.h"


#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EIOInit UPCGExCopyClustersToPointsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }
PCGExData::EIOInit UPCGExCopyClustersToPointsSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::None; }

#pragma endregion

PCGEX_INITIALIZE_ELEMENT(CopyClustersToPoints)

TArray<FPCGPinProperties> UPCGExCopyClustersToPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceTargetsLabel, "Target points to copy clusters to.", Required, {})
	return PinProperties;
}

bool FPCGExCopyClustersToPointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CopyClustersToPoints)

	Context->TargetsDataFacade = PCGExData::TryGetSingleFacade(Context, PCGEx::SourceTargetsLabel, true);
	if (!Context->TargetsDataFacade) { return false; }

	PCGEX_FWD(TransformDetails)
	if (!Context->TransformDetails.Init(Context, Context->TargetsDataFacade.ToSharedRef())) { return false; }

	PCGEX_FWD(TargetsAttributesToPathTags)
	if (!Context->TargetsAttributesToPathTags.Init(Context, Context->TargetsDataFacade)) { return false; }

	Context->TargetsForwardHandler = Settings->TargetsForwarding.GetHandler(Context->TargetsDataFacade);

	return true;
}

bool FPCGExCopyClustersToPointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCopyClustersToPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(CopyClustersToPoints)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExCopyClusters::FBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExCopyClusters::FBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	Context->OutputPointsAndEdges();
	Context->Done();

	return Context->TryComplete();
}

namespace PCGExCopyClusters
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		const TArray<FPCGPoint>& Targets = Context->TargetsDataFacade->GetIn()->GetPoints();
		const int32 NumTargets = Targets.Num();

		PCGEx::InitArray(EdgesDupes, NumTargets);

		for (int i = 0; i < NumTargets; i++)
		{
			// Create an edge copy per target point
			TSharedPtr<PCGExData::FPointIO> EdgeDupe = Context->MainEdges->Emplace_GetRef(EdgeDataFacade->Source, PCGExData::EIOInit::Duplicate);

			EdgesDupes[i] = EdgeDupe;
			PCGExGraph::MarkClusterEdges(EdgeDupe, *(VtxTag->GetData() + i));

			AsyncManager->Start<PCGExGeoTasks::FTransformPointIO>(i, Context->TargetsDataFacade->Source, EdgeDupe, &Context->TransformDetails);
		}

		return true;
	}

	void FProcessor::CompleteWork()
	{
		const TArray<FPCGPoint>& Targets = Context->TargetsDataFacade->GetIn()->GetPoints();
		const int32 NumTargets = Targets.Num();

		// Once work is complete, check if there are cached clusters we can forward
		const TSharedPtr<PCGExCluster::FCluster> CachedCluster = PCGExClusterData::TryGetCachedCluster(VtxDataFacade->Source, EdgeDataFacade->Source);

		for (int i = 0; i < NumTargets; i++)
		{
			TSharedPtr<PCGExData::FPointIO> EdgeDupe = EdgesDupes[i];

			Context->TargetsAttributesToPathTags.Tag(i, EdgeDupe);
			Context->TargetsForwardHandler->Forward(i, EdgeDupe->GetOut()->Metadata);
		}

		if (!CachedCluster) { return; }

		for (int i = 0; i < NumTargets; i++)
		{
			TSharedPtr<PCGExData::FPointIO> VtxDupe = *(VtxDupes->GetData() + i);
			TSharedPtr<PCGExData::FPointIO> EdgeDupe = EdgesDupes[i];

			UPCGExClusterEdgesData* EdgeDupeTypedData = Cast<UPCGExClusterEdgesData>(EdgeDupe->GetOut());
			if (CachedCluster && EdgeDupeTypedData)
			{
				EdgeDupeTypedData->SetBoundCluster(
					MakeShared<PCGExCluster::FCluster>(
						CachedCluster.ToSharedRef(), VtxDupe, EdgeDupe, CachedCluster->NodeIndexLookup,
						false, false, false));
			}
		}
	}

	FBatch::~FBatch()
	{
	}

	void FBatch::Process()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(CopyClustersToPoints)

		const TArray<FPCGPoint>& Targets = Context->TargetsDataFacade->GetIn()->GetPoints();
		const int32 NumTargets = Targets.Num();

		PCGEx::InitArray(VtxDupes, NumTargets);
		VtxTag.Reserve(NumTargets);

		for (int i = 0; i < NumTargets; i++)
		{
			// Create a vtx copy per target point
			TSharedPtr<PCGExData::FPointIO> VtxDupe = Context->MainPoints->Emplace_GetRef(VtxDataFacade->Source, PCGExData::EIOInit::Duplicate);

			FString OutId;
			PCGExGraph::SetClusterVtx(VtxDupe, OutId);

			VtxDupes[i] = VtxDupe;
			VtxTag.Add(OutId);

			AsyncManager->Start<PCGExGeoTasks::FTransformPointIO>(i, Context->TargetsDataFacade->Source, VtxDupe, &Context->TransformDetails);

			Context->TargetsAttributesToPathTags.Tag(i, VtxDupe);
			Context->TargetsForwardHandler->Forward(i, VtxDupe->GetOut()->Metadata);
		}

		TBatch<FProcessor>::Process();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor)
	{
		if (!TBatch<FProcessor>::PrepareSingle(ClusterProcessor)) { return false; }
		ClusterProcessor->VtxDupes = &VtxDupes;
		ClusterProcessor->VtxTag = &VtxTag;
		return true;
	}
}
#undef LOCTEXT_NAMESPACE
