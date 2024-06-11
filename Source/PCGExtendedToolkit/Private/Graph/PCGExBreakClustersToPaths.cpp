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

TArray<FPCGPinProperties> UPCGExBreakClustersToPathsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputPathsLabel, "Paths", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExBreakClustersToPathsSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExBreakClustersToPathsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FName UPCGExBreakClustersToPathsSettings::GetVtxFilterLabel() const { return PCGExDataFilter::SourceFiltersLabel; }


PCGEX_INITIALIZE_ELEMENT(BreakClustersToPaths)

FPCGExBreakClustersToPathsContext::~FPCGExBreakClustersToPathsContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(Paths)
	PCGEX_DELETE_TARRAY(Chains)
}

bool FPCGExBreakClustersToPathsContext::DefaultVtxFilterResult() const { return false; }


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
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (!Context->ProcessorAutomation()) { return false; }

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges) { return false; }
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		if (!Context->AdvanceEdges(true))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		if (!Context->CurrentCluster) { return false; }

		Context->SetState(PCGExDataFilter::State_FilteringPoints);
	}

	if (Context->IsState(PCGExDataFilter::State_FilteringPoints))
	{
		//for (const PCGExCluster::FNode& Node : Context->CurrentCluster->Nodes) { if (Node.IsComplex()) { Context->VtxFilterResults[Node.NodeIndex] = true; } }

		Context->bInvertOrder = Settings->bInvertPathOrder;
		Context->bExecCount = 0;

		if (Settings->OperateOn == EPCGExBreakClusterOperationTarget::Paths)
		{
			Context->GetAsyncManager()->Start<PCGExClusterTask::FFindNodeChains>(
				Context->CurrentEdges->IOIndex, nullptr, Context->CurrentCluster,
				&Context->VtxFilterResults, &Context->Chains, Settings->MinPointCount > 2, false);

			Context->SetAsyncState(PCGExCluster::State_ProcessingChains);
		}
		else
		{
			Context->SetState(PCGExGraph::State_ProcessingEdges);
		}
	}

	auto AdvanceProcessor = [&]()
	{
		if (Context->bExecCount > 0 && Settings->bInvertDuplicateOrder) { Context->bInvertOrder = !Context->bInvertOrder; }

		if ((!Settings->bDuplicatePaths && Context->bExecCount == 1) || Context->bExecCount > 1)
		{
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
		else
		{
			if (Settings->OperateOn == EPCGExBreakClusterOperationTarget::Paths) { Context->SetState(PCGExCluster::State_BuildingChains); }
			else { Context->SetState(PCGExGraph::State_ProcessingEdges); }
		}

		Context->bExecCount++;
	};

	if (Context->IsState(PCGExCluster::State_ProcessingChains))
	{
		PCGEX_WAIT_ASYNC

		PCGExClusterTask::DedupeChains(Context->Chains);

		AdvanceProcessor();
	}

	if (Context->IsState(PCGExCluster::State_BuildingChains))
	{
		auto ProcessChain = [&](const int32 ChainIndex)
		{
			PCGExCluster::FNodeChain* Chain = Context->Chains[ChainIndex];
			if (!Chain) { return; }

			const int32 ChainSize = Chain->Nodes.Num() + 2;

			if (ChainSize < Settings->MinPointCount) { return; }
			if (Settings->bOmitAbovePointCount && ChainSize > Settings->MaxPointCount) { return; }

			const PCGExData::FPointIO& PathIO = Context->Paths->Emplace_GetRef(Context->CurrentIO->GetIn(), PCGExData::EInit::NewOutput);
			TArray<FPCGPoint>& MutablePoints = PathIO.GetOut()->GetMutablePoints();
			MutablePoints.SetNumUninitialized(ChainSize);
			int32 PointCount = 0;

			auto AddPoint = [&](const int32 NodeIndex) { MutablePoints[PointCount++] = PathIO.GetInPoint(Context->CurrentCluster->Nodes[NodeIndex].PointIndex); };

			if (Context->bInvertOrder)
			{
				AddPoint(Chain->Last);
				for (int i = 0; i < Chain->Nodes.Num(); i++) { AddPoint(Chain->Nodes.Last(i)); }
				AddPoint(Chain->First);
			}
			else
			{
				AddPoint(Chain->First);
				for (const int32 NodeIndex : Chain->Nodes) { AddPoint(NodeIndex); }
				AddPoint(Chain->Last);
			}

			PathIO.SetNumInitialized(ChainSize, true);
		};

		if (!Context->Process(ProcessChain, Context->Chains.Num())) { return false; }

		AdvanceProcessor();
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		auto ProcessEdge = [&](const int32 EdgeIndex)
		{
			const PCGExData::FPointIO& PathIO = Context->Paths->Emplace_GetRef(Context->CurrentIO->GetIn(), PCGExData::EInit::NewOutput);
			TArray<FPCGPoint>& MutablePoints = PathIO.GetOut()->GetMutablePoints();
			MutablePoints.SetNumUninitialized(2);

			const PCGExGraph::FIndexedEdge& Edge = Context->CurrentCluster->Edges[EdgeIndex];

			if (Context->bInvertOrder)
			{
				MutablePoints[0] = PathIO.GetInPoint(Edge.End);
				MutablePoints[1] = PathIO.GetInPoint(Edge.Start);
			}
			else
			{
				MutablePoints[0] = PathIO.GetInPoint(Edge.Start);
				MutablePoints[1] = PathIO.GetInPoint(Edge.End);
			}

			PathIO.SetNumInitialized(2, true);
		};

		if (!Context->Process(ProcessEdge, Context->CurrentCluster->Edges.Num())) { return false; }

		AdvanceProcessor();
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
