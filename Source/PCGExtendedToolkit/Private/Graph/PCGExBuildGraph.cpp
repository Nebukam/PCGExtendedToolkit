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

	Context->bComputeEdgeType = Settings->bComputeEdgeType;
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
		if (Context->CurrentIO)
		{
			//Cleanup current PointIO, indices won't be needed anymore.
			Context->CurrentIO->Flush();
		}

		if (!Context->AdvancePointsIO(true))
		{
			Context->Done(); //No more points
		}
		else
		{
			//Context->CurrentIO->BuildMetadataEntriesAndIndices(); // Required to retrieve index when using the Octree
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
		auto Initialize = [&](const UPCGExPointIO* PointIO)
		{
			Context->PrepareCurrentGraphForPoints(PointIO->Out, Context->bComputeEdgeType);
		};

		auto ProcessPoint = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
		{
			Context->CreateAndStartTask<FProbeTask>(PointIndex, PointIO->GetInPoint(PointIndex).MetadataEntry);
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint))
		{
			Context->SetState(PCGExMT::State_WaitingOnAsyncWork);
		}
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (Context->IsAsyncWorkComplete())
		{
			if (Context->bComputeEdgeType) { Context->SetState(PCGExGraph::State_FindingEdgeTypes); }
			else { Context->SetState(PCGExGraph::State_ReadyForNextGraph); }
		}
	}

	if (Context->IsState(PCGExGraph::State_FindingEdgeTypes))
	{
		// Process params again for edges types
		auto ProcessPointEdgeType = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
		{
			PCGExGraph::ComputeEdgeType(Context->SocketInfos, PointIO->GetOutPoint(PointIndex), PointIndex, PointIO);
		};

		if (Context->ProcessCurrentPoints(ProcessPointEdgeType))
		{
			Context->SetState(PCGExGraph::State_ReadyForNextGraph);
		}
	}

	if (Context->IsDone())
	{
		Context->OutputPointsAndParams();
		return true;
	}

	return false;
}

void FProbeTask::ExecuteTask()
{
	const FPCGExBuildGraphContext* Context = static_cast<FPCGExBuildGraphContext*>(TaskContext);

	const FPCGPoint& Point = PointData->GetOutPoint(Infos.Index);
	Context->CachedIndex->SetValue(Point.MetadataEntry, Infos.Index); // Cache index

	TArray<PCGExGraph::FSocketProbe> Probes;
	const double MaxDistance = Context->GraphSolver->PrepareProbesForPoint(Context->SocketInfos, Point, Probes);

	const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Point.Transform.GetLocation(), FVector(MaxDistance));
	/*
	Context->Octree->FindElementsWithBoundsTest(
		Box, [&](const FPCGPointRef& OtherPointRef)
		{
			const FPCGPoint& OtherPoint = *OtherPointRef.Point;
			int Index = PointData->GetIndex(OtherPoint.MetadataEntry);
			if (Index == Infos.Index) { return; }
			for (PCGExGraph::FSocketProbe& Probe : Probes) { Context->GraphSolver->ProcessPoint(Probe, OtherPoint, Index); }
		});
	*/

	// This looks bad, smells bad, feels bad, but for some reason it's ~100-200 times faster than using the Octree.
	const FBox BBox = Box.GetBox();
	const TArray<FPCGPoint>& InPoints = PointData->In->GetPoints();
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

	ExecutionComplete(true);
}

#undef LOCTEXT_NAMESPACE
