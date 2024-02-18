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

	PCGEX_DELETE(HelperCluster)
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
		PCGEX_DELETE(Context->HelperCluster)

		if (!Context->AdvancePointsIOAndResetGraph()) { Context->Done(); }
		else
		{
			Context->CurrentIO->CreateOutKeys();

			if (Settings->bConnectivityBasedSearch)
			{
				Context->GetAsyncManager()->Start<FPCGExBuildGraphHelperTask>(-1, Context->CurrentIO);
				Context->SetAsyncState(PCGExGeo::State_ProcessingDelaunay);
			}
			else
			{
				Context->SetAsyncState(PCGExGraph::State_ReadyForNextGraph);
			}
		}
	}

	if (Context->IsState(PCGExGeo::State_ProcessingDelaunay))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		Context->SetState(PCGExGraph::State_ReadyForNextGraph);
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph())
		{
			Context->CurrentIO->Cleanup();
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		if (!Context->PrepareCurrentGraphForPoints(*Context->CurrentIO, false))
		{
			PCGEX_GRAPH_MISSING_METADATA
			return false;
		}

		for (int i = 0; i < Context->CurrentIO->GetNum(); i++)
		{
			Context->GetAsyncManager()->Start<FPCGExProbeTask>(i, Context->CurrentIO);
		}

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

bool FPCGExBuildGraphHelperTask::ExecuteTask()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildGraphHelperTask::ExecuteTask);

	FPCGExBuildCustomGraphContext* Context = Manager->GetContext<FPCGExBuildCustomGraphContext>();

	const TArray<FPCGPoint>& Points = PointIO->GetIn()->GetPoints();
	TArray<FVector> Positions;
	PCGExGeo::PointsToPositions(Points, Positions);

	// Add a bit of random to avoid collinear/coplanar points
	const FVector One = FVector(1);
	const FVector MinusOne = FVector(-1);
	for (int i = 0; i < Points.Num(); i++)
	{
		FVector Pos = Points[i].Transform.GetLocation();
		const double Size = FMath::PerlinNoise3D(PCGExMath::Tile(Pos * 0.001 + i, MinusOne, One)) * 0.01;
		Positions[i] = Pos + FVector(Size);
	}

	PCGExGeo::TDelaunay3* Delaunay = new PCGExGeo::TDelaunay3();

	const TArrayView<FVector> View = MakeArrayView(Positions);
	if (Delaunay->Process(View))
	{
		Context->HelperCluster = new PCGExCluster::FCluster();
		Context->HelperCluster->BuildPartialFrom(Positions, Delaunay->DelaunayEdges);
	}

	PCGEX_DELETE(Delaunay)
	return false;
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
	//const UPCGPointData::PointOctree& Octree = Context->CurrentIO->GetIn()->GetOctree();

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

	if (Context->HelperCluster && !Context->HelperCluster->Edges.IsEmpty())
	{
		TArray<int32> Neighbors;
		Neighbors.Reserve(20);
		Neighbors.Add(TaskIndex);

		Context->HelperCluster->GetConnectedNodes(TaskIndex, Neighbors, Settings->SearchDepth);

		for (const int32 i : Neighbors)
		{
			PCGExCluster::FNode& Node = Context->HelperCluster->Nodes[i];
			if (!Box.IsInside(Node.Position)) { continue; }

			if (i == TaskIndex) { continue; }

			const PCGEx::FPointRef& OtherPoint = PointIO->GetOutPointRef(i);

			for (PCGExGraph::FSocketProbe& Probe : Probes)
			{
				Context->GraphSolver->ProcessPoint(Probe, OtherPoint);
			}
		}
	}
	else
	{
		// Fallback if helper couldn't be built
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
