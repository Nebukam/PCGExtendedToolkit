// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildCustomGraph.h"

#define LOCTEXT_NAMESPACE "PCGExBuildCustomGraph"
#define PCGEX_NAMESPACE BuildCustomGraph

int32 UPCGExBuildCustomGraphSettings::GetPreferredChunkSize() const { return 32; }
PCGExData::EInit UPCGExBuildCustomGraphSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGExBuildCustomGraphContext::~FPCGExBuildCustomGraphContext()
{
	PCGEX_TERMINATE_ASYNC
}

UPCGExBuildCustomGraphSettings::UPCGExBuildCustomGraphSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PCGEX_OPERATION_DEFAULT(GraphSolver, UPCGExGraphSolver)
}

#if WITH_EDITOR
void UPCGExBuildCustomGraphSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (GraphSolver) { GraphSolver->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

FName UPCGExBuildCustomGraphSettings::GetMainInputLabel() const { return PCGEx::SourcePointsLabel; }

PCGEX_INITIALIZE_ELEMENT(BuildCustomGraph)

bool FPCGExBuildCustomGraphElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExCustomGraphProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildCustomGraph)

	PCGEX_OPERATION_BIND(GraphSolver, UPCGExCustomGraphSolver)

	return true;
}

bool FPCGExBuildCustomGraphElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildCustomGraphElement::Execute);

	PCGEX_CONTEXT(BuildCustomGraph)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	// Prep point for param loops
	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIOAndResetGraph()) { Context->Done(); }
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
			PointIO.CreateInKeys();
			PointIO.CreateOutKeys();
			Context->PrepareCurrentGraphForPoints(PointIO, false);
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			Context->GetAsyncManager()->Start<FProbeTask>(PointIndex, Context->CurrentIO);
		};

		if (!Context->ProcessCurrentPoints(Initialize, ProcessPoint)) { return false; }
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		Context->SetState(PCGExGraph::State_FindingEdgeTypes);
	}

	if (Context->IsState(PCGExGraph::State_FindingEdgeTypes))
	{
		// Process params again for edges types
		auto ProcessPointEdgeType = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			ComputeEdgeType(Context->SocketInfos, PointIndex);
		};

		if (!Context->ProcessCurrentPoints(ProcessPointEdgeType)) { return false; }
		for (const PCGExGraph::FSocketInfos& SocketInfos : Context->SocketInfos) { SocketInfos.Socket->Write(); }
		Context->SetState(PCGExGraph::State_ReadyForNextGraph);
	}

	if (Context->IsDone()) { Context->OutputPointsAndGraphParams(); }

	return Context->IsDone();
}

bool FProbeTask::ExecuteTask()
{
	const FPCGExBuildCustomGraphContext* Context = Manager->GetContext<FPCGExBuildCustomGraphContext>();


	const PCGEx::FPointRef Point = PCGEx::FPointRef(PointIO->GetOutPoint(TaskIndex), TaskIndex);

	Context->SetCachedIndex(TaskIndex, TaskIndex);

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

		Probe.OutputTo(Point.Index);
		PCGEX_CLEANUP(Probe)
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
