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
		for (int i = 0; i < Context->CurrentCluster->Nodes.Num(); i++) { if (Context->CurrentCluster->Nodes[i].IsComplex()) { Context->VtxFilterResults[i] = true; } }

		if (Settings->OperateOn == EPCGExBreakClusterOperationTarget::Paths)
		{
			Context->GetAsyncManager()->Start<PCGExClusterTask::FFindNodeChains>(
				Context->CurrentEdges->IOIndex, nullptr, Context->CurrentCluster,
				&Context->VtxFilterResults, &Context->Chains, Settings->MinPointCount > 2, false);

			Context->SetAsyncState(PCGExCluster::State_BuildingChains);
		}
		else
		{
			Context->SetState(PCGExGraph::State_ProcessingEdges);
		}
	}

	if (Context->IsState(PCGExCluster::State_BuildingChains))
	{
		PCGEX_WAIT_ASYNC

		auto Initialize = [&]() { PCGExClusterTask::DedupeChains(Context->Chains); };

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

			AddPoint(Chain->First);
			for (const int32 NodeIndex : Chain->Nodes) { AddPoint(NodeIndex); }
			AddPoint(Chain->Last);


			PathIO.SetNumInitialized(ChainSize, true);

			if(Settings->bAddInverseDuplicate)
			{
				// TODO Implement
			}
			
		};

		if (!Context->Process(Initialize, ProcessChain, Context->Chains.Num())) { return false; }
		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		auto ProcessEdge = [&](const int32 EdgeIndex)
		{
			const PCGExData::FPointIO& PathIO = Context->Paths->Emplace_GetRef(Context->CurrentIO->GetIn(), PCGExData::EInit::NewOutput);
			TArray<FPCGPoint>& MutablePoints = PathIO.GetOut()->GetMutablePoints();
			MutablePoints.SetNumUninitialized(2);

			const PCGExGraph::FIndexedEdge& Edge = Context->CurrentCluster->Edges[EdgeIndex];
			MutablePoints[0] = PathIO.GetInPoint(Edge.Start);
			MutablePoints[1] = PathIO.GetInPoint(Edge.End);

			PathIO.SetNumInitialized(2, true);

			if(Settings->bAddInverseDuplicate)
			{
				const PCGExData::FPointIO& DupePathIO = Context->Paths->Emplace_GetRef(Context->CurrentIO->GetIn(), PCGExData::EInit::NewOutput);
				TArray<FPCGPoint>& DupeMutablePoints = DupePathIO.GetOut()->GetMutablePoints();
				DupeMutablePoints.SetNumUninitialized(2);

				DupeMutablePoints[1] = PathIO.GetInPoint(Edge.Start);
				DupeMutablePoints[0] = PathIO.GetInPoint(Edge.End);

				DupePathIO.SetNumInitialized(2, true);
			}
		};

		if (!Context->Process(ProcessEdge, Context->CurrentCluster->Edges.Num())) { return false; }
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
