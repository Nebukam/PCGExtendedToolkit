﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingFindClusterHull.h"

#include "Data/PCGExData.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointIO.h"

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
	PCGEX_PIN_POINTS(PCGExPaths::OutputPathsLabel, "Hulls", Required)
	return PinProperties;
}

PCGExData::EIOInit UPCGExFindClusterHullSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::NoInit; }
PCGExData::EIOInit UPCGExFindClusterHullSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NoInit; }

PCGEX_INITIALIZE_ELEMENT(FindClusterHull)
PCGEX_ELEMENT_BATCH_EDGE_IMPL(FindClusterHull)

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
		if (!Context->StartProcessingClusters(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
			{
				// NewBatch->bRequiresWriteStep = true;
				NewBatch->SetProjectionDetails(Settings->ProjectionDetails);
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::State_Done)

	// TODO : Output seeds?

	Context->Paths->StageOutputs();

	return Context->TryComplete();
}


namespace PCGExFindClusterHull
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFindClusterHull::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		const TArray<FVector2D>& Proj = *ProjectedVtxPositions.Get();

		CellsConstraints = MakeShared<PCGExTopology::FCellConstraints>(Settings->Constraints);
		CellsConstraints->BuildWrapperCell(Cluster.ToSharedRef(), Proj, CellsConstraints);

		if (!CellsConstraints->WrapperCell)
		{
			if (!Settings->bQuietFailedToFindHullWarning) { PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Failed to find the hull of a cluster.")); }
			return false;
		}

		ProcessCell(CellsConstraints->WrapperCell);

		CellsConstraints->Cleanup();
		CellsConstraints.Reset();

		return true;
	}

	void FProcessor::ProcessCell(const TSharedPtr<PCGExTopology::FCell>& InCell)
	{
		const TSharedPtr<PCGExData::FPointIO> PathIO = Context->Paths->Emplace_GetRef<UPCGPointArrayData>(VtxDataFacade->Source, PCGExData::EIOInit::New);
		if (!PathIO) { return; }

		PathIO->Tags->Reset();                                          // Tag forwarding handled by artifacts
		PathIO->IOIndex = Cluster->GetEdge(InCell->Seed.Edge)->IOIndex; // Enforce seed order for collection output-ish

		PCGExGraph::CleanupClusterData(PathIO);

		PCGEX_MAKE_SHARED(PathDataFacade, PCGExData::FFacade, PathIO.ToSharedRef())

		TArray<int32> ReadIndices;
		ReadIndices.SetNumUninitialized(InCell->Nodes.Num());

		for (int i = 0; i < InCell->Nodes.Num(); i++) { ReadIndices[i] = Cluster->GetNodePointIndex(InCell->Nodes[i]); }
		PathIO->InheritPoints(ReadIndices, 0);
		InCell->PostProcessPoints(PathIO->GetOut());

		PCGExPaths::SetClosedLoop(PathDataFacade->GetOut(), true);

		Context->Artifacts.Process(Cluster, PathDataFacade, InCell);
		PathDataFacade->WriteFastest(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
