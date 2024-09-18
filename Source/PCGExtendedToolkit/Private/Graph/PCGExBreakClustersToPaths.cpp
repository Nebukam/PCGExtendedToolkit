// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBreakClustersToPaths.h"

#include "Graph/Filters/PCGExClusterFilter.h"

#define LOCTEXT_NAMESPACE "PCGExBreakClustersToPaths"
#define PCGEX_NAMESPACE BreakClustersToPaths

TArray<FPCGPinProperties> UPCGExBreakClustersToPathsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputPathsLabel, "Paths", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExBreakClustersToPathsSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExBreakClustersToPathsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(BreakClustersToPaths)

FPCGExBreakClustersToPathsContext::~FPCGExBreakClustersToPathsContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(Paths)
	PCGEX_DELETE_TARRAY(Chains)
}

bool FPCGExBreakClustersToPathsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BreakClustersToPaths)

	Context->Paths = new PCGExData::FPointIOCollection(Context);
	Context->Paths->DefaultOutputLabel = PCGExGraph::OutputPathsLabel;

	return true;
}

bool FPCGExBreakClustersToPathsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBreakClustersToPathsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BreakClustersToPaths)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExBreakClustersToPaths::FProcessorBatch>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExBreakClustersToPaths::FProcessorBatch* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	Context->Paths->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExBreakClustersToPaths
{
	FProcessor::~FProcessor()
	{
		Breakpoints.Empty();
		PCGEX_DELETE_TARRAY(Chains)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBreakClustersToPaths::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BreakClustersToPaths)

		LocalTypedContext = TypedContext;
		LocalSettings = Settings;

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		Breakpoints.Init(false, Cluster->Nodes->Num());

		if (!DirectionSettings.InitFromParent(Context, GetParentBatch<FProcessorBatch>()->DirectionSettings, EdgeDataFacade))
		{
			return false;
		}

		if (!TypedContext->FilterFactories.IsEmpty())
		{
			PCGExClusterFilter::TManager* FilterManager = new PCGExClusterFilter::TManager(Cluster, VtxDataFacade, EdgeDataFacade);
			FilterManager->Init(Context, TypedContext->FilterFactories);
			for (const PCGExCluster::FNode& Node : *Cluster->Nodes) { Breakpoints[Node.NodeIndex] = Node.IsComplex() ? true : FilterManager->Test(Node); }
			PCGEX_DELETE(FilterManager)
		}
		else
		{
			for (const PCGExCluster::FNode& Node : *Cluster->Nodes) { Breakpoints[Node.NodeIndex] = Node.IsComplex(); }
		}

		if (Settings->OperateOn == EPCGExBreakClusterOperationTarget::Paths)
		{
			AsyncManagerPtr->Start<PCGExClusterTask::FFindNodeChains>(
				EdgesIO->IOIndex, nullptr, Cluster,
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
		PCGEX_SETTINGS(BreakClustersToPaths)
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
		PCGExCluster::FNodeChain* Chain = Chains[Iteration];
		if (!Chain) { return; }

		const int32 ChainSize = Chain->Nodes.Num() + 2;

		const TArray<int32>& VtxPointsIndicesRef = *VtxPointIndicesCache;

		int32 StartIdx = VtxPointsIndicesRef[Chain->First];
		int32 EndIdx = VtxPointsIndicesRef[Chain->Last];

		bool bReverse = false;

		if (DirectionSettings.DirectionMethod == EPCGExEdgeDirectionMethod::EdgeDotAttribute)
		{
			PCGExGraph::FIndexedEdge ChainDir = PCGExGraph::FIndexedEdge((*Cluster->Nodes)[Chain->First].GetEdgeIndex(Chain->Last), StartIdx, EndIdx);
			bReverse = DirectionSettings.SortEndpoints(Cluster, ChainDir);
		}
		else
		{
			PCGExGraph::FIndexedEdge ChainDir = PCGExGraph::FIndexedEdge(Iteration, StartIdx, EndIdx);
			bReverse = DirectionSettings.SortEndpoints(Cluster, ChainDir);
		}

		if (bReverse)
		{
			Algo::Reverse(Chain->Nodes);
			std::swap(StartIdx, EndIdx);
		}

		if (ChainSize < LocalSettings->MinPointCount) { return; }
		if (LocalSettings->bOmitAbovePointCount && ChainSize > LocalSettings->MaxPointCount) { return; }

		const PCGExData::FPointIO* PathIO = LocalTypedContext->Paths->Emplace_GetRef<UPCGPointData>(VtxIO, PCGExData::EInit::NewOutput);

		TArray<FPCGPoint>& MutablePoints = PathIO->GetOut()->GetMutablePoints();
		MutablePoints.SetNumUninitialized(ChainSize);
		int32 PointCount = 0;

		MutablePoints[PointCount++] = PathIO->GetInPoint(StartIdx);
		for (const int32 NodeIndex : Chain->Nodes) { MutablePoints[PointCount++] = PathIO->GetInPoint(VtxPointsIndicesRef[NodeIndex]); }
		MutablePoints[PointCount] = PathIO->GetInPoint(EndIdx);

		PathIO->InitializeNum(ChainSize, true);
	}

	void FProcessor::ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count)
	{
		const PCGExData::FPointIO* PathIO = LocalTypedContext->Paths->Emplace_GetRef<UPCGPointData>(VtxIO, PCGExData::EInit::NewOutput);
		TArray<FPCGPoint>& MutablePoints = PathIO->GetOut()->GetMutablePoints();
		MutablePoints.SetNumUninitialized(2);

		DirectionSettings.SortEndpoints(Cluster, Edge);

		MutablePoints[0] = PathIO->GetInPoint(Edge.Start);
		MutablePoints[1] = PathIO->GetInPoint(Edge.End);

		PathIO->InitializeNum(2, true);
	}

	void FProcessorBatch::OnProcessingPreparationComplete()
	{
		VtxDataFacade->bSupportsDynamic = true;

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BreakClustersToPaths)

		DirectionSettings = Settings->DirectionSettings;
		if (!DirectionSettings.Init(Context, VtxDataFacade))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some vtx are missing the specified Direction attribute."));
			return;
		}

		if (DirectionSettings.RequiresEndpointsMetadata())
		{
			// Fetch attributes while processors are searching for chains

			const int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize();

			PCGEX_ASYNC_GROUP(AsyncManagerPtr, FetchVtxTask)
			FetchVtxTask->SetOnIterationRangeStartCallback(
				[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
				{
					VtxDataFacade->Fetch(StartIndex, Count);
				});

			FetchVtxTask->PrepareRangesOnly(VtxIO->GetNum(), PLI);
		}

		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}
}


#undef LOCTEXT_NAMESPACE


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
