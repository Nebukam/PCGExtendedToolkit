// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildGraph.h"

#define LOCTEXT_NAMESPACE "PCGExBuildGraph"

int32 UPCGExBuildGraphSettings::GetPreferredChunkSize() const { return 32; }
PCGExPointIO::EInit UPCGExBuildGraphSettings::GetPointOutputInitMode() const { return PCGExPointIO::EInit::DuplicateInput; }

UPCGExBuildGraphSettings::UPCGExBuildGraphSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GraphSolver = EnsureInstruction<UPCGExGraphSolver>(GraphSolver);
}

void UPCGExBuildGraphSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	GraphSolver = EnsureInstruction<UPCGExGraphSolver>(GraphSolver);
	if (GraphSolver) { GraphSolver->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

FPCGElementPtr UPCGExBuildGraphSettings::CreateElement() const { return MakeShared<FPCGExBuildGraphElement>(); }
FName UPCGExBuildGraphSettings::GetMainPointsInputLabel() const { return PCGEx::SourcePointsLabel; }

FPCGContext* FPCGExBuildGraphElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExBuildGraphContext* Context = new FPCGExBuildGraphContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExBuildGraphSettings* Settings = Context->GetInputSettings<UPCGExBuildGraphSettings>();
	check(Settings);

	Context->GraphSolver = Settings->EnsureInstruction<UPCGExGraphSolver>(Settings->GraphSolver, Context);

	return Context;
}

bool FPCGExBuildGraphElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildGraphElement::Execute);

	FPCGExBuildGraphContext* Context = static_cast<FPCGExBuildGraphContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	// Prep point for param loops
	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO(true))
		{
			Context->Done(); //No more points
		}
		else
		{
			Context->CurrentIO->Cleanup();
			Context->CurrentIO->GetInKeys();
			Context->CurrentIO->BuildMetadataEntries();
			Context->SetState(PCGExGraph::State_ReadyForNextGraph);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph())
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}
		Context->SetState(State_ProbingPoints);
	}

	// Process params for current points


	if (Context->IsState(State_ProbingPoints))
	{
		auto Initialize = [&](FPCGExPointIO& PointIO)
		{
			Context->PrepareCurrentGraphForPoints(PointIO.GetOut(), true);
		};

		auto ProcessPoint = [&](const int32 PointIndex, const FPCGExPointIO& PointIO)
		{
			//TODO : Use async to compute results but DO NOT WRITE ON ATTRIBUTES

			//Context->GetAsyncManager()->StartTask<FProbeTask>(PointIndex, PointIO->GetInPoint(PointIndex).MetadataEntry, Context->CurrentIO);

			const FPCGPoint& Point = PointIO.GetOutPoint(PointIndex);
			Context->CachedIndex->SetValue(Point.MetadataEntry, PointIndex); // Cache index

			TArray<PCGExGraph::FSocketProbe> Probes;
			const double MaxDistance = Context->GraphSolver->PrepareProbesForPoint(Context->SocketInfos, Point, Probes);

			const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Point.Transform.GetLocation(), FVector(MaxDistance));

			//TODO : This is what needs to be async.
			// This looks bad, but for some reason it's MUCH faster than using the Octree.
			const FBox BBox = Box.GetBox();
			const TArray<FPCGPoint>& InPoints = PointIO.GetIn()->GetPoints();
			for (int i = 0; i < InPoints.Num(); i++)
			{
				if (const FPCGPoint& Pt = InPoints[i];
					BBox.IsInside(Pt.Transform.GetLocation()))
				{
					for (PCGExGraph::FSocketProbe& Probe : Probes) { Context->GraphSolver->ProcessPoint(Probe, Pt, i); }
				}
			}

			const PCGMetadataEntryKey Key = Point.MetadataEntry;
			for (PCGExGraph::FSocketProbe& Probe : Probes)
			{
				Context->GraphSolver->ResolveProbe(Probe);
				Probe.OutputTo(Key);
			}
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
		auto ProcessPointEdgeType = [&](const int32 PointIndex, const FPCGExPointIO& PointIO)
		{
			ComputeEdgeType(Context->SocketInfos, PointIO.GetOutPoint(PointIndex), PointIndex, PointIO);
		};

		if (Context->ProcessCurrentPoints(ProcessPointEdgeType)) { Context->SetState(PCGExGraph::State_ReadyForNextGraph); }
	}

	if (Context->IsDone())
	{
		Context->OutputPointsAndGraphParams();
		return true;
	}

	return false;
}

bool FProbeTask::ExecuteTask()
{
	const FPCGExBuildGraphContext* Context = Manager->GetContext<FPCGExBuildGraphContext>();
	PCGEX_ASYNC_CHECKPOINT

	const FPCGPoint& Point = PointIO->GetOutPoint(TaskInfos.Index);
	Context->CachedIndex->SetValue(Point.MetadataEntry, TaskInfos.Index); // Cache index

	TArray<PCGExGraph::FSocketProbe> Probes;
	const double MaxDistance = Context->GraphSolver->PrepareProbesForPoint(Context->SocketInfos, Point, Probes);

	const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Point.Transform.GetLocation(), FVector(MaxDistance));

	// This looks bad, but for some reason it's MUCH faster than using the Octree.
	const FBox BBox = Box.GetBox();
	const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
	for (int i = 0; i < InPoints.Num(); i++)
	{
		if (const FPCGPoint& Pt = InPoints[i];
			BBox.IsInside(Pt.Transform.GetLocation()))
		{
			for (PCGExGraph::FSocketProbe& Probe : Probes) { Context->GraphSolver->ProcessPoint(Probe, Pt, i); }
		}
	}

	const PCGMetadataEntryKey Key = Point.MetadataEntry;
	for (PCGExGraph::FSocketProbe& Probe : Probes)
	{
		PCGEX_ASYNC_CHECKPOINT
		Context->GraphSolver->ResolveProbe(Probe);
		Probe.OutputTo(Key);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
