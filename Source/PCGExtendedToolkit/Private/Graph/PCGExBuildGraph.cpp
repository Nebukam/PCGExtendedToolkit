// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildGraph.h"

#define LOCTEXT_NAMESPACE "PCGExBuildGraph"

int32 UPCGExBuildGraphSettings::GetPreferredChunkSize() const { return 32; }
PCGExIO::EInitMode UPCGExBuildGraphSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::DuplicateInput; }
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
	Context->bSimpleMode = Settings->bSimpleMode;

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
		Context->SetState(PCGExMT::EState::ReadyForNextPoints);
	}

	// Prep point for param loops

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		if (Context->CurrentIO)
		{
			//Cleanup current PointIO, indices won't be needed anymore.
			Context->CurrentIO->Flush();
		}

		if (!Context->AdvancePointsIO(true))
		{
			Context->SetState(PCGExMT::EState::Done); //No more points
		}
		else
		{
			Context->CurrentIO->BuildMetadataEntriesAndIndices();
			Context->Octree = const_cast<UPCGPointData::PointOctree*>(&(Context->CurrentIO->Out->GetOctree())); // Not sure this really saves perf
			Context->SetState(PCGExMT::EState::ReadyForNextGraph);
		}
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph())
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
			return false;
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingGraph);
		}
	}

	// Process params for current points

	auto ProcessPoint = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
	{
		const FPCGPoint& Point = PointIO->GetOutPoint(PointIndex);
		Context->CachedIndex->SetValue(Point.MetadataEntry, PointIndex); // Cache index

		TArray<PCGExGraph::FSocketProbe> Probes;
		const double MaxDistance = Context->PrepareProbesForPoint(Point, Probes);

		auto ProcessPointNeighbor = [&](const FPCGPointRef& OtherPointRef)
		{
			const FPCGPoint* OtherPoint = OtherPointRef.Point;
			const int32 Index = PointIO->GetIndex(OtherPoint->MetadataEntry);

			if (Index == PointIndex) { return; }
			if (Context->bSimpleMode)
			{
				for (PCGExGraph::FSocketProbe& Probe : Probes) { Probe.ProcessPointSimple(OtherPoint, Index); }
			}
			else
			{
				for (PCGExGraph::FSocketProbe& Probe : Probes) { Probe.ProcessPointComplex(OtherPoint, Index); }
			}
		};

		const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Point.Transform.GetLocation(), FVector(MaxDistance));
		Context->Octree->FindElementsWithBoundsTest(Box, ProcessPointNeighbor);

		const PCGMetadataEntryKey Key = Point.MetadataEntry;
		if (Context->bSimpleMode)
		{
			for (PCGExGraph::FSocketProbe& Probe : Probes)
			{
				Probe.OutputTo(Key);
			}
		}
		else
		{
			for (PCGExGraph::FSocketProbe& Probe : Probes)
			{
				Probe.ProcessCandidates();
				Probe.OutputTo(Key);
			}
		}
	};

	auto Initialize = [&](const UPCGExPointIO* PointIO)
	{
		Context->PrepareCurrentGraphForPoints(PointIO->Out, Context->bComputeEdgeType);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingGraph))
	{
		if (Context->AsyncProcessingCurrentPoints(Initialize, ProcessPoint))
		{
			if (Context->bComputeEdgeType) { Context->SetState(PCGExMT::EState::ProcessingGraph2ndPass); }
			else { Context->SetState(PCGExMT::EState::ReadyForNextGraph); }
		}
	}

	// Process params again for edges types
	auto ProcessPointForGraph = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
	{
		Context->ComputeEdgeType(PointIO->GetOutPoint(PointIndex), PointIndex, PointIO);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingGraph2ndPass))
	{
		if (Context->AsyncProcessingCurrentPoints(ProcessPointForGraph))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextGraph);
		}
	}

	if (Context->IsState(PCGExMT::EState::Done))
	{
		Context->OutputPointsAndParams();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
