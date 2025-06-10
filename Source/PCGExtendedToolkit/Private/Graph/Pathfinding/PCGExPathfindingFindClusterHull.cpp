// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingFindClusterHull.h"

#define LOCTEXT_NAMESPACE "PCGExFindClusterHull"
#define PCGEX_NAMESPACE FindClusterHull

TArray<FPCGPinProperties> UPCGExFindClusterHullSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExFindClusterHullSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExPaths::OutputPathsLabel, "Hulls", Required, {})
	return PinProperties;
}

PCGExData::EIOInit UPCGExFindClusterHullSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::NoInit; }
PCGExData::EIOInit UPCGExFindClusterHullSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NoInit; }

PCGEX_INITIALIZE_ELEMENT(FindClusterHull)

bool FPCGExFindClusterHullElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindClusterHull)

	PCGEX_FWD(Artifacts)
	if (!Context->Artifacts.Init(Context)) { return false; }

	Context->Paths = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->Paths->OutputPin = PCGExPaths::OutputPathsLabel;

	return true;
}

bool FPCGExFindClusterHullElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindClusterHullElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FindClusterHull)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExFindClusterHull::FBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExFindClusterHull::FBatch>& NewBatch)
			{
				// NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	// TODO : Output seeds?

	Context->Paths->StageOutputs();

	return Context->TryComplete();
}


namespace PCGExFindClusterHull
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFindClusterHull::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		CellsConstraints = MakeShared<PCGExTopology::FCellConstraints>(Settings->Constraints);

		const FVector SeedWP = Cluster->Bounds.GetCenter() + Cluster->Bounds.GetSize() * 1.01;

		const TSharedPtr<PCGExTopology::FCell> Cell = MakeShared<PCGExTopology::FCell>(CellsConstraints.ToSharedRef());
		const PCGExTopology::ECellResult Result = Cell->BuildFromCluster(SeedWP, Cluster.ToSharedRef(), *ProjectedPositions);
		if (Result != PCGExTopology::ECellResult::Success)
		{
			if (!Settings->bQuietFailedToFindHullWarning){ PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Failed to find the hull of a cluster.")); }
			return false;
		}

		ProcessCell(Cell);

		return true;
	}

	void FProcessor::ProcessCell(const TSharedPtr<PCGExTopology::FCell>& InCell)
	{
		const TSharedPtr<PCGExData::FPointIO> PathIO = Context->Paths->Emplace_GetRef<UPCGPointArrayData>(VtxDataFacade->Source, PCGExData::EIOInit::New);
		if (!PathIO) { return; }

		PathIO->Tags->Reset();                                          // Tag forwarding handled by artifacts
		PathIO->IOIndex = Cluster->GetEdge(InCell->Seed.Edge)->IOIndex; // Enforce seed order for collection output-ish

		PCGExGraph::CleanupClusterTags(PathIO);
		PCGExGraph::CleanupVtxData(PathIO);

		PCGEX_MAKE_SHARED(PathDataFacade, PCGExData::FFacade, PathIO.ToSharedRef())

		TArray<int32> ReadIndices;
		ReadIndices.SetNumUninitialized(InCell->Nodes.Num());

		for (int i = 0; i < InCell->Nodes.Num(); i++) { ReadIndices[i] = Cluster->GetNode(InCell->Nodes[i])->PointIndex; }
		PathIO->InheritPoints(ReadIndices, 0);
		InCell->PostProcessPoints(PathIO->GetOut());

		Context->Artifacts.Process(Cluster, PathDataFacade, InCell);
		PathDataFacade->WriteFastest(AsyncManager);
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExFindClusterHullContext, UPCGExFindClusterHullSettings>::Cleanup();
		CellsConstraints->Cleanup();
	}

	void FBatch::Process()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FindClusterHull)

		// Project positions
		ProjectionDetails = Settings->ProjectionDetails;
		if (!ProjectionDetails.Init(Context, VtxDataFacade)) { return; }

		PCGEx::InitArray(ProjectedPositions, VtxDataFacade->GetNum());

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ProjectionTaskGroup)

		ProjectionTaskGroup->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->OnProjectionComplete();
			};

		ProjectionTaskGroup->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				TArray<FVector>& PP = *This->ProjectedPositions;
				This->ProjectionDetails.ProjectFlat(This->VtxDataFacade, PP, Scope);
			};

		ProjectionTaskGroup->StartSubLoops(VtxDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}

	bool FBatch::PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor)
	{
		ClusterProcessor->ProjectedPositions = ProjectedPositions;
		TBatch<FProcessor>::PrepareSingle(ClusterProcessor);
		return true;
	}

	void FBatch::OnProjectionComplete()
	{
		TBatch<FProcessor>::Process();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
