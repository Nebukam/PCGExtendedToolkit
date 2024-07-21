// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCopyClustersToPoints.h"


#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExCopyClustersToPointsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExCopyClustersToPointsSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

#pragma endregion

FPCGExCopyClustersToPointsContext::~FPCGExCopyClustersToPointsContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(Targets)
}

PCGEX_INITIALIZE_ELEMENT(CopyClustersToPoints)

TArray<FPCGPinProperties> UPCGExCopyClustersToPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceTargetsLabel, "Target points to copy clusters to.", Required, {})
	return PinProperties;
}

bool FPCGExCopyClustersToPointsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CopyClustersToPoints)

	PCGEX_FWD(TransformDetails)

	Context->Targets = PCGExData::TryGetSingleInput(Context, PCGEx::SourceTargetsLabel, true);
	if (!Context->Targets) { return false; }

	return true;
}

bool FPCGExCopyClustersToPointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCopyClustersToPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(CopyClustersToPoints)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		if (!Context->StartProcessingClusters<PCGExCopyClusters::FBatch>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExCopyClusters::FBatch* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	Context->OutputPointsAndEdges();
	Context->Done();

	return Context->TryComplete();
}

namespace PCGExCopyClusters
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(CopyClustersToPoints)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		LocalTypedContext = TypedContext;

		const TArray<FPCGPoint>& Targets = TypedContext->Targets->GetIn()->GetPoints();
		const int32 NumTargets = Targets.Num();

		PCGEX_SET_NUM_UNINITIALIZED(EdgesDupes, NumTargets)

		for (int i = 0; i < NumTargets; i++)
		{
			// Create an edge copy per target point
			PCGExData::FPointIO* EdgeDupe = LocalTypedContext->MainEdges->Emplace_GetRef(EdgesIO, PCGExData::EInit::DuplicateInput);

			EdgesDupes[i] = EdgeDupe;
			PCGExGraph::MarkClusterEdges(EdgeDupe, *(VtxTag->GetData() + i));

			AsyncManagerPtr->Start<PCGExGeoTasks::FTransformPointIO>(i, TypedContext->Targets, EdgeDupe, &TypedContext->TransformDetails);
		}

		return true;
	}

	void FProcessor::CompleteWork()
	{
		// Once work is complete, check if there are cached clusters we can forward
		const PCGExCluster::FCluster* CachedCluster = PCGExClusterData::TryGetCachedCluster(VtxIO, EdgesIO);

		if (!CachedCluster) { return; }

		const TArray<FPCGPoint>& Targets = LocalTypedContext->Targets->GetIn()->GetPoints();
		const int32 NumTargets = Targets.Num();

		for (int i = 0; i < NumTargets; i++)
		{
			PCGExData::FPointIO* VtxDupe = *(VtxDupes->GetData() + i);
			PCGExData::FPointIO* EdgeDupe = EdgesDupes[i];

			UPCGExClusterEdgesData* EdgeDupeTypedData = Cast<UPCGExClusterEdgesData>(EdgeDupe->GetOut());
			if (CachedCluster && EdgeDupeTypedData)
			{
				PCGExCluster::FCluster* ClusterCopy = new PCGExCluster::FCluster(
					CachedCluster, VtxDupe, EdgeDupe, false, false, false);

				EdgeDupeTypedData->SetBoundCluster(ClusterCopy, true);
			}
		}
	}

	FBatch::~FBatch()
	{
	}

	void FBatch::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(CopyClustersToPoints)

		LocalTypedContext = TypedContext;

		const TArray<FPCGPoint>& Targets = TypedContext->Targets->GetIn()->GetPoints();
		const int32 NumTargets = Targets.Num();

		PCGEX_SET_NUM_UNINITIALIZED(VtxDupes, NumTargets)
		VtxTag.Reserve(NumTargets);

		for (int i = 0; i < NumTargets; i++)
		{
			// Create a vtx copy per target point
			PCGExData::FPointIO* VtxDupe = LocalTypedContext->MainPoints->Emplace_GetRef(VtxIO, PCGExData::EInit::DuplicateInput);

			FString OutId;
			PCGExGraph::SetClusterVtx(VtxDupe, OutId);

			VtxDupes[i] = VtxDupe;
			VtxTag.Add(OutId);

			AsyncManager->Start<PCGExGeoTasks::FTransformPointIO>(i, TypedContext->Targets, VtxDupe, &TypedContext->TransformDetails);
		}

		TBatch<FProcessor>::Process(AsyncManager);
	}

	bool FBatch::PrepareSingle(FProcessor* ClusterProcessor)
	{
		if (!TBatch<FProcessor>::PrepareSingle(ClusterProcessor)) { return false; }
		ClusterProcessor->VtxDupes = &VtxDupes;
		ClusterProcessor->VtxTag = &VtxTag;
		return true;
	}

}
#undef LOCTEXT_NAMESPACE
