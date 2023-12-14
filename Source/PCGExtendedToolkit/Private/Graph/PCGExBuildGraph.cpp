// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildGraph.h"

#define LOCTEXT_NAMESPACE "PCGExBuildGraph"
#define PCGEX_NAMESPACE BuildGraph

int32 UPCGExBuildGraphSettings::GetPreferredChunkSize() const { return 32; }
PCGExData::EInit UPCGExBuildGraphSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGExBuildGraphContext::~FPCGExBuildGraphContext()
{
	PCGEX_TERMINATE_ASYNC
}

UPCGExBuildGraphSettings::UPCGExBuildGraphSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GraphSolver = EnsureOperation<UPCGExGraphSolver>(GraphSolver);
}

void UPCGExBuildGraphSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	GraphSolver = EnsureOperation<UPCGExGraphSolver>(GraphSolver);
	if (GraphSolver) { GraphSolver->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

FPCGElementPtr UPCGExBuildGraphSettings::CreateElement() const { return MakeShared<FPCGExBuildGraphElement>(); }
FName UPCGExBuildGraphSettings::GetMainInputLabel() const { return PCGEx::SourcePointsLabel; }

PCGEX_INITIALIZE_CONTEXT(BuildGraph)

bool FPCGExBuildGraphElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExGraphProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildGraph)

	PCGEX_BIND_OPERATION(GraphSolver, UPCGExGraphSolver)

	return true;
}

bool FPCGExBuildGraphElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildGraphElement::Execute);

	PCGEX_CONTEXT(BuildGraph)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	// Prep point for param loops
	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO(true)) { Context->Done(); }
		else { Context->SetState(PCGExGraph::State_ReadyForNextGraph); }
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph())
		{
			Context->CurrentIO->Cleanup();
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}
		Context->SetState(State_ProbingPoints);
	}

	// Process params for current points


	if (Context->IsState(State_ProbingPoints))
	{
		auto Initialize = [&](PCGExData::FPointIO& PointIO)
		{
			Context->PrepareCurrentGraphForPoints(PointIO, false);
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			Context->GetAsyncManager()->Start<FProbeTask>(PointIndex, Context->CurrentIO);
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint)) { Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork); }
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (Context->IsAsyncWorkComplete()) { Context->SetState(PCGExGraph::State_FindingEdgeTypes); }
	}

	if (Context->IsState(PCGExGraph::State_FindingEdgeTypes))
	{
		// Process params again for edges types
		auto ProcessPointEdgeType = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			ComputeEdgeType(Context->SocketInfos, PointIndex);
		};

		if (Context->ProcessCurrentPoints(ProcessPointEdgeType))
		{
			for (const PCGExGraph::FSocketInfos& SocketInfos : Context->SocketInfos) { SocketInfos.Socket->Write(); }
			Context->SetState(PCGExGraph::State_ReadyForNextGraph);
		}
	}

	if (Context->IsDone()) { Context->OutputPointsAndGraphParams(); }

	return Context->IsDone();
}

bool FProbeTask::ExecuteTask()
{
	const FPCGExBuildGraphContext* Context = Manager->GetContext<FPCGExBuildGraphContext>();
	PCGEX_ASYNC_CHECKPOINT

	const PCGEx::FPointRef Point = PCGEx::FPointRef(PointIO->GetOutPoint(TaskInfos.Index), TaskInfos.Index);

	Context->SetCachedIndex(TaskInfos.Index, TaskInfos.Index);

	TArray<PCGExGraph::FSocketProbe> Probes;
	const double MaxDistance = Context->GraphSolver->PrepareProbesForPoint(Context->SocketInfos, Point, Probes);

	const FBox Box = FBoxCenterAndExtent(Point.Point->Transform.GetLocation(), FVector(MaxDistance)).GetBox();

	const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
	for (int i = 0; i < InPoints.Num(); i++)
	{
		if (const FPCGPoint& Pt = InPoints[i];
			Box.IsInside(Pt.Transform.GetLocation()))
		{
			const PCGEx::FPointRef& OtherPoint = PointIO->GetOutPointRef(i);
			for (PCGExGraph::FSocketProbe& Probe : Probes) { Context->GraphSolver->ProcessPoint(Probe, OtherPoint); }
		}
	}

	for (PCGExGraph::FSocketProbe& Probe : Probes)
	{
		Context->GraphSolver->ResolveProbe(Probe);
		PCGEX_ASYNC_CHECKPOINT
		Probe.OutputTo(Point.Index);
		PCGEX_CLEANUP(Probe)
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
