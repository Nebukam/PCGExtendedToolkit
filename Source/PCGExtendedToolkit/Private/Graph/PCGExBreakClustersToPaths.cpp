// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBreakClustersToPaths.h"


#include "Graph/PCGExChain.h"
#include "Graph/Filters/PCGExClusterFilter.h"

#define LOCTEXT_NAMESPACE "PCGExBreakClustersToPaths"
#define PCGEX_NAMESPACE BreakClustersToPaths

TArray<FPCGPinProperties> UPCGExBreakClustersToPathsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputPathsLabel, "Paths", Required, {})
	return PinProperties;
}

PCGExData::EIOInit UPCGExBreakClustersToPathsSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::NoOutput; }
PCGExData::EIOInit UPCGExBreakClustersToPathsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(BreakClustersToPaths)

bool FPCGExBreakClustersToPathsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BreakClustersToPaths)

	Context->Paths = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->Paths->DefaultOutputLabel = PCGExGraph::OutputPathsLabel;

	return true;
}

bool FPCGExBreakClustersToPathsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBreakClustersToPathsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BreakClustersToPaths)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExBreakClustersToPaths::FProcessorBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExBreakClustersToPaths::FProcessorBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	Context->Paths->StageOutputs();
	return Context->TryComplete();
}

namespace PCGExBreakClustersToPaths
{
	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBreakClustersToPaths::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		Breakpoints.Init(0, Cluster->Nodes->Num());

		if (!DirectionSettings.InitFromParent(ExecutionContext, StaticCastWeakPtr<FProcessorBatch>(ParentBatch).Pin()->DirectionSettings, EdgeDataFacade))
		{
			return false;
		}

		const TSharedPtr<PCGExClusterFilter::FManager> FilterManager = MakeShared<PCGExClusterFilter::FManager>(Cluster.ToSharedRef(), VtxDataFacade, EdgeDataFacade);
		if (!Context->FilterFactories.IsEmpty() && FilterManager->Init(ExecutionContext, Context->FilterFactories))
		{
			for (const PCGExCluster::FNode& Node : *Cluster->Nodes) { Breakpoints[Node.NodeIndex] = Node.IsComplex() ? true : FilterManager->Test(Node); }
		}
		else
		{
			for (const PCGExCluster::FNode& Node : *Cluster->Nodes) { Breakpoints[Node.NodeIndex] = Node.IsComplex(); }
		}

		if (Settings->OperateOn == EPCGExBreakClusterOperationTarget::Paths)
		{
			AsyncManager->Start<PCGExClusterTask::FFindNodeChains>(
				EdgeDataFacade->Source->IOIndex, nullptr, Cluster,
				&Breakpoints, &Chains, false);
		}
		else
		{
			// TODO : Break individual edges
		}

		return true;
	}


	void FProcessor::CompleteWork()
	{
		if (Settings->OperateOn == EPCGExBreakClusterOperationTarget::Paths)
		{
			PCGExClusterTask::DedupeChains(Chains);
			StartParallelLoopForRange(Chains.Num());
		}
		else
		{
			StartParallelLoopForEdges();
		}
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 Count)
	{
		const TSharedPtr<PCGExCluster::FNodeChain> Chain = Chains[Iteration];
		if (!Chain) { return; }

		const int32 ChainSize = Chain->Nodes.Num() + 2; // Skip last point

		int32 StartIdx = Cluster->GetNodePointIndex(Chain->First);
		int32 EndIdx = Cluster->GetNodePointIndex(Chain->Last);

		bool bReverse = false;

		if (DirectionSettings.DirectionMethod == EPCGExEdgeDirectionMethod::EdgeDotAttribute)
		{
			// BUG possible bug with this, First isn't guaranteed to be linked to Last
			PCGExGraph::FIndexedEdge ChainDir = PCGExGraph::FIndexedEdge((*Cluster->Nodes)[Chain->First].GetEdgeIndex(Chain->Last), StartIdx, EndIdx);
			bReverse = DirectionSettings.SortEndpoints(Cluster.Get(), ChainDir);
		}
		else
		{
			PCGExGraph::FIndexedEdge ChainDir = PCGExGraph::FIndexedEdge(Iteration, StartIdx, EndIdx);
			bReverse = DirectionSettings.SortEndpoints(Cluster.Get(), ChainDir);
		}

		if (bReverse)
		{
			Algo::Reverse(Chain->Nodes);
			std::swap(StartIdx, EndIdx);
		}

		if (ChainSize < Settings->MinPointCount) { return; }
		if (Settings->bOmitAbovePointCount && ChainSize > Settings->MaxPointCount) { return; }

		const TSharedPtr<PCGExData::FPointIO> PathIO = Context->Paths->Emplace_GetRef<UPCGPointData>(VtxDataFacade->Source, PCGExData::EIOInit::NewOutput);

		TArray<FPCGPoint>& MutablePoints = PathIO->GetOut()->GetMutablePoints();
		MutablePoints.SetNumUninitialized(Chain->bClosedLoop ? ChainSize - 1 : ChainSize);
		int32 PointCount = 0;

		MutablePoints[PointCount++] = PathIO->GetInPoint(StartIdx);
		for (const int32 NodeIndex : Chain->Nodes) { MutablePoints[PointCount++] = *Cluster->GetNodePoint(NodeIndex); }

		if (!Chain->bClosedLoop)
		{
			MutablePoints[PointCount] = PathIO->GetInPoint(EndIdx); // Add last
			if (Settings->bTagIfOpenPath) { PathIO->Tags->Add(Settings->IsOpenPathTag); }
		}
		else
		{
			if (Settings->bTagIfClosedLoop) { PathIO->Tags->Add(Settings->IsClosedLoopTag); }
		}
	}

	void FProcessor::ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count)
	{
		const TSharedPtr<PCGExData::FPointIO> PathIO = Context->Paths->Emplace_GetRef<UPCGPointData>(VtxDataFacade->Source, PCGExData::EIOInit::NewOutput);
		TArray<FPCGPoint>& MutablePoints = PathIO->GetOut()->GetMutablePoints();
		MutablePoints.SetNumUninitialized(2);

		DirectionSettings.SortEndpoints(Cluster.Get(), Edge);

		MutablePoints[0] = PathIO->GetInPoint(Edge.Start);
		MutablePoints[1] = PathIO->GetInPoint(Edge.End);
	}

	void FProcessorBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BreakClustersToPaths)
		PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->FilterFactories, FacadePreloader);
		DirectionSettings.RegisterBuffersDependencies(ExecutionContext, FacadePreloader);
	}

	void FProcessorBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BreakClustersToPaths)

		DirectionSettings = Settings->DirectionSettings;
		if (!DirectionSettings.Init(Context, VtxDataFacade, Context->GetEdgeSortingRules()))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some vtx are missing the specified Direction attribute."));
			return;
		}

		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}
}


#undef LOCTEXT_NAMESPACE


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
