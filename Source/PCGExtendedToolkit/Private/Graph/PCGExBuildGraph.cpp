// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildGraph.h"

#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "PCGExBuildGraph"

int32 UPCGExBuildGraphSettings::GetPreferredChunkSize() const { return 32; }

PCGEx::EIOInit UPCGExBuildGraphSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

FPCGElementPtr UPCGExBuildGraphSettings::CreateElement() const
{
	return MakeShared<FPCGExBuildGraphElement>();
}

FPCGContext* FPCGExBuildGraphElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExBuildGraphContext* Context = new FPCGExBuildGraphContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

bool FPCGExBuildGraphElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildGraphElement::Execute);

	FPCGExBuildGraphContext* Context = static_cast<FPCGExBuildGraphContext*>(InContext);

	const UPCGExBuildGraphSettings* Settings = Context->GetInputSettings<UPCGExBuildGraphSettings>();
	check(Settings);

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
			//Cleanup current IO, indices won't be needed anymore.
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

	// Process params for current points

	auto ProcessPoint = [&Context](const FPCGPoint& Point, const int32 ReadIndex, const UPCGExPointIO* IO)
	{
		Context->CachedIndex->SetValue(Point.MetadataEntry, ReadIndex); // Cache index

		TArray<PCGExGraph::FSocketProbe> Probes;
		const double MaxDistance = Context->PrepareProbesForPoint(Point, Probes);
		PCGExGraph::FPointCandidate Candidate;

		auto ProcessPointNeighbor = [&ReadIndex, &Probes, &IO, &Candidate](const FPCGPointRef& OtherPointRef)
		{
			const FPCGPoint* OtherPoint = OtherPointRef.Point;
			const int32 Index = IO->GetIndex(OtherPoint->MetadataEntry);

			if (Index == ReadIndex) { return; }
			for (PCGExGraph::FSocketProbe& Probe : Probes) { Probe.ProcessPoint(OtherPoint, Index); }
		};

		const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Point.Transform.GetLocation(), FVector(MaxDistance));
		Context->Octree->FindElementsWithBoundsTest(Box, ProcessPointNeighbor);

		const PCGMetadataEntryKey Key = Point.MetadataEntry;
		for (PCGExGraph::FSocketProbe& Probe : Probes)
		{
			Probe.ProcessCandidates();
			Probe.OutputTo(Key);
		}
	};

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

	auto Initialize = [&Context, &Settings](const UPCGExPointIO* IO)
	{
		Context->CurrentGraph->PrepareForPointData(IO->Out, Settings->bComputeEdgeType);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingGraph))
	{
		if (Context->CurrentIO->OutputParallelProcessing(Context, Initialize, ProcessPoint, Context->ChunkSize))
		{
			if (Settings->bComputeEdgeType)
			{
				Context->SetState(PCGExMT::EState::ProcessingGraph2ndPass);
			}
			else
			{
				Context->SetState(PCGExMT::EState::ReadyForNextGraph);
			}
		}
	}

	// Process params again for edges types

	auto InitializeForGraph = [](UPCGExPointIO* IO)
	{
	};

	auto ProcessPointForGraph = [&Context](const FPCGPoint& Point, const int32 ReadIndex, const UPCGExPointIO* IO)
	{
		Context->ComputeEdgeType(Point, ReadIndex, IO);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingGraph2ndPass))
	{
		if (Context->CurrentIO->OutputParallelProcessing(Context, InitializeForGraph, ProcessPointForGraph, Context->ChunkSize))
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
