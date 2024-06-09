// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildCustomGraph.h"

#include "Geometry/PCGExGeo.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExBuildCustomGraph"
#define PCGEX_NAMESPACE BuildCustomGraph

int32 UPCGExBuildCustomGraphSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_M; }
PCGExData::EInit UPCGExBuildCustomGraphSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGExBuildCustomGraphContext::~FPCGExBuildCustomGraphContext()
{
	PCGEX_TERMINATE_ASYNC
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

void UPCGExBuildCustomGraphSettings::PostInitProperties()
{
	Super::PostInitProperties();
	PCGEX_OPERATION_DEFAULT(GraphSolver, UPCGExCustomGraphSolver)
}

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

	PCGEX_CONTEXT_AND_SETTINGS(BuildCustomGraph)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	// Prep point for param loops
	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIOAndResetGraph()) { Context->Done(); }
		else
		{
			Context->CurrentIO->CreateOutKeys();
			Context->SetAsyncState(PCGExGraph::State_ReadyForNextGraph);
		}
	}

	if (Context->IsState(PCGExGeo::State_ProcessingDelaunay))
	{
		PCGEX_WAIT_ASYNC
		Context->SetState(PCGExGraph::State_ReadyForNextGraph);
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph())
		{
			Context->CurrentIO->CleanupKeys();
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		if (!Context->PrepareCurrentGraphForPoints(*Context->CurrentIO, false))
		{
			PCGEX_GRAPH_MISSING_METADATA
			return false;
		}

		/*
		for (int i = 0; i < Context->CurrentIO->GetNum(); i++)
		{
			Context->GetAsyncManager()->Start<FPCGExProbeTask>(i, Context->CurrentIO);
		}
		*/
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		//PCGEX_WAIT_ASYNC

		auto ProcessProbe = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			const PCGEx::FPointRef Point = PCGEx::FPointRef(PointIO.GetOutPoint(PointIndex), PointIndex);

			Context->SetCachedIndex(PointIndex, PointIndex);

			TArray<PCGExGraph::FSocketProbe> Probes;
			const double MaxRadius = Context->GraphSolver->PrepareProbesForPoint(Context->SocketInfos, Point, Probes);

			const FBoxCenterAndExtent BoxCAE = FBoxCenterAndExtent(Point.Point->Transform.GetLocation(), FVector(MaxRadius));

			const TArray<FPCGPoint>& InPoints = PointIO.GetIn()->GetPoints();

			auto ProcessPoint = [&](const FPCGPointRef& InPointRef)
			{
				const ptrdiff_t OtherPointIndex = InPointRef.Point - InPoints.GetData();

				if (!InPoints.IsValidIndex(static_cast<int32>(OtherPointIndex)) ||
					static_cast<int32>(OtherPointIndex) == PointIndex) { return; }

				const PCGEx::FPointRef& OtherPoint = PointIO.GetOutPointRef(OtherPointIndex);
				for (PCGExGraph::FSocketProbe& Probe : Probes) { Context->GraphSolver->ProcessPoint(Probe, OtherPoint); }
			};

			const UPCGPointData::PointOctree& Octree = Context->GetCurrentIn()->GetOctree();
			Octree.FindElementsWithBoundsTest(BoxCAE, ProcessPoint);

			for (PCGExGraph::FSocketProbe& Probe : Probes)
			{
				Context->GraphSolver->ResolveProbe(Probe);
				Probe.OutputTo(Point.Index);
				PCGEX_CLEANUP(Probe)
			}
		};

		if (!Context->ProcessCurrentPoints(ProcessProbe)) { return false; }

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

		Context->WriteSocketInfos();
		Context->SetState(PCGExGraph::State_ReadyForNextGraph);
	}

	if (Context->IsDone())
	{
		Context->OutputPointsAndGraphParams();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

bool FPCGExProbeTask::ExecuteTask()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExProbeTask::ExecuteTask);

	const FPCGExBuildCustomGraphContext* Context = Manager->GetContext<FPCGExBuildCustomGraphContext>();
	PCGEX_SETTINGS(BuildCustomGraph)

	const PCGEx::FPointRef Point = PCGEx::FPointRef(PointIO->GetOutPoint(TaskIndex), TaskIndex);

	Context->SetCachedIndex(TaskIndex, TaskIndex);

	TArray<PCGExGraph::FSocketProbe> Probes;
	const double MaxRadius = Context->GraphSolver->PrepareProbesForPoint(Context->SocketInfos, Point, Probes);

	const FBoxCenterAndExtent BoxCAE = FBoxCenterAndExtent(Point.Point->Transform.GetLocation(), FVector(MaxRadius));
	const FBox Box = BoxCAE.GetBox();

	const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
	//const UPCGPointData::PointOctree& Octree = Context->GetCurrentIn()->GetOctree();

	/*
	auto ProcessPoint = [&](const FPCGPointRef& InPointRef)
	{
		const ptrdiff_t PointIndex = InPointRef.Point - InPoints.GetData();
		if (PointIndex < 0 && PointIndex >= InPoints.Num() || static_cast<int32>(PointIndex) == TaskIndex) { return; }

		const PCGEx::FPointRef& OtherPoint = PointIO->GetOutPointRef(PointIndex);
		
		for (PCGExGraph::FSocketProbe& Probe : Probes)
		{
			Context->GraphSolver->ProcessPoint(Probe, OtherPoint);
		}

	};
	
	Octree.FindElementsWithBoundsTest(BoxCAE, ProcessPoint);
	*/

	auto ProcessPoint = [&](const FPCGPointRef& InPointRef)
	{
		const ptrdiff_t PointIndex = InPointRef.Point - InPoints.GetData();
		if (!InPoints.IsValidIndex(PointIndex) || PointIndex == TaskIndex) { return; }

		const PCGEx::FPointRef& OtherPoint = PointIO->GetOutPointRef(PointIndex);

		for (PCGExGraph::FSocketProbe& Probe : Probes)
		{
			Context->GraphSolver->ProcessPoint(Probe, OtherPoint);
		}
	};

	const UPCGPointData::PointOctree& Octree = Context->GetCurrentIn()->GetOctree();
	Octree.FindElementsWithBoundsTest(BoxCAE, ProcessPoint);


	/*
	for (int i = 0; i < InPoints.Num(); i++)
	{
		if (const FPCGPoint& Pt = InPoints[i];
			!Box.IsInside(Pt.Transform.GetLocation())) { continue; }

		if (i == TaskIndex) { continue; }

		const PCGEx::FPointRef& OtherPoint = PointIO->GetOutPointRef(i);

		for (PCGExGraph::FSocketProbe& Probe : Probes)
		{
			Context->GraphSolver->ProcessPoint(Probe, OtherPoint);
		}
	}
	*/


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
