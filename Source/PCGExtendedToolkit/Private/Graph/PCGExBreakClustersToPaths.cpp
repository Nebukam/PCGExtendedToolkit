// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBreakClustersToPaths.h"

#define LOCTEXT_NAMESPACE "PCGExBreakClustersToPaths"
#define PCGEX_NAMESPACE BreakClustersToPaths

UPCGExBreakClustersToPathsSettings::UPCGExBreakClustersToPathsSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPCGExBreakClustersToPathsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGExDataFilter::SourceFiltersLabel, "When using edge chain operation, these filter are used to find additional split points", Advanced, {})
	return PinProperties;
}

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

	PCGEX_DELETE(PointFilterManager)
	PointFilterFactories.Empty();
}


bool FPCGExBreakClustersToPathsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BreakClustersToPaths)

	PCGExFactories::GetInputFactories(InContext, PCGEx::SourcePointFilters, Context->PointFilterFactories, PCGExFactories::ClusterFilters, false);

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
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no associated edges."));
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
				return false;
			}

			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	auto ExportChainsToPaths = [&]()
	{
		Context->GetAsyncManager()->Start<FPCGExBreakClusterTask>(-1, nullptr, Context->CurrentCluster);
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	};

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		PCGEX_DELETE(Context->PointFilterManager)

		if (!Context->AdvanceEdges(true))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		if (!Context->CurrentCluster) { return false; }

		if (!Context->PointFilterFactories.IsEmpty())
		{
			Context->PointFilterManager = new PCGExDataFilter::TEarlyExitFilterManager(Context->CurrentIO);
			Context->PointFilterManager->Register<UPCGExFilterFactoryBase>(Context, Context->PointFilterFactories, Context->CurrentIO);
			if (Context->PointFilterManager->PrepareForTesting()) { Context->SetState(PCGExDataFilter::State_PreparingFilters); }
			else { Context->SetState(PCGExDataFilter::State_FilteringPoints); }
		}
		else { ExportChainsToPaths(); }
	}

	if (Context->IsState(PCGExDataFilter::State_PreparingFilters))
	{
		auto PreparePoint = [&](const int32 NodeIndex) { Context->PointFilterManager->PrepareSingle(Context->CurrentCluster->Nodes[NodeIndex].PointIndex); };

		if (!Context->Process(PreparePoint, Context->CurrentCluster->Nodes.Num())) { return false; }

		Context->PointFilterManager->PreparationComplete();

		Context->SetState(PCGExDataFilter::State_FilteringPoints);
	}

	if (Context->IsState(PCGExDataFilter::State_FilteringPoints))
	{
		auto ProcessPoint = [&](const int32 NodeIndex) { Context->PointFilterManager->Test(Context->CurrentCluster->Nodes[NodeIndex].PointIndex); };

		if (!Context->Process(ProcessPoint, Context->CurrentCluster->Nodes.Num())) { return false; }

		ExportChainsToPaths();
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC

		// TODO : Output chains as new pointIOs, do this async as well

		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsDone())
	{
		Context->Paths->OutputTo(Context);
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

bool FPCGExBreakClusterTask::ExecuteTask()
{
	FPCGExBreakClustersToPathsContext* Context = static_cast<FPCGExBreakClustersToPathsContext*>(Manager->Context);
	PCGEX_SETTINGS(BreakClustersToPaths)


	// TODO find chains

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
