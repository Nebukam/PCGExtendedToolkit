// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBreakClustersToPaths.h"

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

FName UPCGExBreakClustersToPathsSettings::GetVtxFilterLabel() const { return PCGExPointFilter::SourceFiltersLabel; }


PCGEX_INITIALIZE_ELEMENT(BreakClustersToPaths)

FPCGExBreakClustersToPathsContext::~FPCGExBreakClustersToPathsContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(Paths)
	PCGEX_DELETE_TARRAY(Chains)
}

bool FPCGExBreakClustersToPathsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BreakClustersToPaths)

	Context->Paths = new PCGExData::FPointIOCollection();
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

		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatch<PCGExBreakClustersToPaths::FProcessor>>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExClusterMT::TBatch<PCGExBreakClustersToPaths::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	if (Context->IsDone())
	{
		Context->Paths->OutputTo(Context);
	}

	return Context->TryComplete();
}

namespace PCGExBreakClustersToPaths
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE_TARRAY(Chains)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		PCGEX_SETTINGS(BreakClustersToPaths)
		//FPCGExBreakClustersToPathsContext* InContext = static_cast<FPCGExBreakClustersToPathsContext*>(Context);

		const TArray<PCGExCluster::FNode>& NodesRef = *Cluster->Nodes;

		if (Settings->OperateOn == EPCGExBreakClusterOperationTarget::Paths)
		{
			for (int i = 0; i < NodesRef.Num(); i++) { if (NodesRef[i].IsComplex()) { VtxFilterCache[i] = true; } }

			AsyncManagerPtr->Start<PCGExClusterTask::FFindNodeChains>(
				EdgesIO->IOIndex, nullptr, Cluster,
				&VtxFilterCache, &Chains, false);
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

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration)
	{
		PCGEX_SETTINGS(BreakClustersToPaths)
		const FPCGExBreakClustersToPathsContext* InContext = static_cast<FPCGExBreakClustersToPathsContext*>(Context);

		PCGExCluster::FNodeChain* Chain = Chains[Iteration];
		if (!Chain) { return; }

		const int32 ChainSize = Chain->Nodes.Num() + 2;

		if (ChainSize < Settings->MinPointCount) { return; }
		if (Settings->bOmitAbovePointCount && ChainSize > Settings->MaxPointCount) { return; }

		const PCGExData::FPointIO* PathIO = InContext->Paths->Emplace_GetRef<UPCGPointData>(VtxIO->GetIn(), PCGExData::EInit::NewOutput);
		TArray<FPCGPoint>& MutablePoints = PathIO->GetOut()->GetMutablePoints();
		MutablePoints.SetNumUninitialized(ChainSize);
		int32 PointCount = 0;

		const TArray<int32>& VtxPointsIndicesRef = *VtxPointIndicesCache;

		MutablePoints[PointCount++] = PathIO->GetInPoint(VtxPointsIndicesRef[Chain->First]);
		for (const int32 NodeIndex : Chain->Nodes) { MutablePoints[PointCount++] = PathIO->GetInPoint(VtxPointsIndicesRef[NodeIndex]); }
		MutablePoints[PointCount] = PathIO->GetInPoint(VtxPointsIndicesRef[Chain->Last]);

		PathIO->InitializeNum(ChainSize, true);
	}

	void FProcessor::ProcessSingleEdge(PCGExGraph::FIndexedEdge& Edge)
	{
		PCGEX_SETTINGS(BreakClustersToPaths)
		const FPCGExBreakClustersToPathsContext* InContext = static_cast<FPCGExBreakClustersToPathsContext*>(Context);

		const PCGExData::FPointIO* PathIO = InContext->Paths->Emplace_GetRef<UPCGPointData>(VtxIO->GetIn(), PCGExData::EInit::NewOutput);
		TArray<FPCGPoint>& MutablePoints = PathIO->GetOut()->GetMutablePoints();
		MutablePoints.SetNumUninitialized(2);

		MutablePoints[0] = PathIO->GetInPoint(Edge.Start);
		MutablePoints[1] = PathIO->GetInPoint(Edge.End);


		PathIO->InitializeNum(2, true);
	}
}


#undef LOCTEXT_NAMESPACE


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
