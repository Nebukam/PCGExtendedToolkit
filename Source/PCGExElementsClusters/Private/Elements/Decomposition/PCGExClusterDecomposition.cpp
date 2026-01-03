// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/Decomposition/PCGExClusterDecomposition.h"


#include "Data/PCGExData.h"
#include "Clusters/PCGExCluster.h"
#include "Clusters/PCGExClustersHelpers.h"
#include "Data/PCGExPointIO.h"
#include "Elements/Decomposition/PCGExSimpleConvexDecomposer.h"
#include "Math/PCGExBestFitPlane.h"

#define LOCTEXT_NAMESPACE "ClusterDecomposition"
#define PCGEX_NAMESPACE ClusterDecomposition

PCGExData::EIOInit UPCGExClusterDecompositionSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }
PCGExData::EIOInit UPCGExClusterDecompositionSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Forward; }

PCGEX_INITIALIZE_ELEMENT(ClusterDecomposition)
PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(ClusterDecomposition)

bool FPCGExClusterDecompositionElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ClusterDecomposition)

	return true;
}

bool FPCGExClusterDecompositionElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExClusterDecompositionElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ClusterDecomposition)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters([](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; }, [&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
		{
			NewBatch->bRequiresWriteStep = true;
		}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExClusterDecomposition
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExClusterDecomposition::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

		// Run decomposition
		PCGExClusters::FSimpleConvexDecomposer Decomposer;
		PCGExClusters::FConvexDecomposition Result;

		if (Decomposer.Decompose(Cluster.Get(), Result, Settings->DecompositionSettings))
		{
			const int32 Offset = EdgeDataFacade->Source->IOIndex * 1000000;
			for (int32 i = 0; i < Result.Cells.Num(); i++)
			{
				const PCGExClusters::FConvexCell3D& Cell = Result.Cells[i];
				for (const int32 NodeIndex : Cell.NodeIndices) { CellIDBuffer->SetValue(Cluster->GetNodePointIndex(NodeIndex), Offset + i); }
			}
		}

		return true;
	}

	void FProcessor::CompleteWork()
	{
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExClusterDecompositionContext, UPCGExClusterDecompositionSettings>::Cleanup();
	}

	//////// BATCH

	FBatch::FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
		: TBatch(InContext, InVtx, InEdges)
	{
	}

	FBatch::~FBatch()
	{
	}

	void FBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ClusterDecomposition)

		CellIDBuffer = VtxDataFacade->GetWritable<int32>(Settings->CellIDAttributeName, -1, true, PCGExData::EBufferInit::New);

		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor)
	{
		if (!TBatch<FProcessor>::PrepareSingle(InProcessor)) { return false; }
		PCGEX_TYPED_PROCESSOR

		TypedProcessor->CellIDBuffer = CellIDBuffer;

		return true;
	}

	void FBatch::Write()
	{
		VtxDataFacade->WriteFastest(TaskManager);
		TBatch<FProcessor>::Write();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
