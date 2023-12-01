// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildGraph.h"

#define LOCTEXT_NAMESPACE "PCGExBuildGraph"

int32 UPCGExBuildGraphSettings::GetPreferredChunkSize() const { return 32; }
PCGExIO::EInitMode UPCGExBuildGraphSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::DuplicateInput; }

UPCGExBuildGraphSettings::UPCGExBuildGraphSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!GraphSolver) { GraphSolver = NewObject<UPCGExGraphSolver>(); }
}

TArray<FPCGPinProperties> UPCGExBuildGraphSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	if(bComputePatches)
	{
		FPCGPinProperties& PinPatchesOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputPatchesLabel, EPCGDataType::Point);

#if WITH_EDITOR
		PinPatchesOutput.Tooltip = LOCTEXT("PCGExOutputPatchTooltip", "Patches");
#endif // WITH_EDITOR
	}
	
	return PinProperties;
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
	Context->GraphSolver = Settings->GraphSolver;

	if (!Context->GraphSolver) { Context->GraphSolver = NewObject<UPCGExGraphSolver>(); }

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
			Context->SetState(PCGExMT::State_Done); //No more points
		}
		else
		{
			Context->CurrentIO->BuildMetadataEntriesAndIndices();
			Context->Octree = const_cast<UPCGPointData::PointOctree*>(&(Context->CurrentIO->Out->GetOctree())); // Not sure this really saves perf
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

	auto ProcessPoint = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
	{
		const FPCGPoint& Point = PointIO->GetOutPoint(PointIndex);
		Context->CachedIndex->SetValue(Point.MetadataEntry, PointIndex); // Cache index

		TArray<PCGExGraph::FSocketProbe> Probes;
		const double MaxDistance = Context->GraphSolver->PrepareProbesForPoint(Context->SocketInfos, Point, Probes);

		auto ProcessPointNeighbor = [&](const FPCGPointRef& OtherPointRef)
		{
			const FPCGPoint* OtherPoint = OtherPointRef.Point;
			const int32 Index = PointIO->GetIndex(OtherPoint->MetadataEntry);

			if (Index == PointIndex) { return; }
			for (PCGExGraph::FSocketProbe& Probe : Probes) { Context->GraphSolver->ProcessPoint(Probe, OtherPoint, Index); }
		};

		const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Point.Transform.GetLocation(), FVector(MaxDistance));
		Context->Octree->FindElementsWithBoundsTest(Box, ProcessPointNeighbor);

		const PCGMetadataEntryKey Key = Point.MetadataEntry;
		for (PCGExGraph::FSocketProbe& Probe : Probes)
		{
			Context->GraphSolver->ResolveProbe(Probe);
			Probe.OutputTo(Key);
		}
	};

	auto Initialize = [&](const UPCGExPointIO* PointIO)
	{
		Context->PrepareCurrentGraphForPoints(PointIO->Out, Context->bComputeEdgeType);
	};

	if (Context->IsState(State_ProbingPoints))
	{
		if (Context->AsyncProcessingCurrentPoints(Initialize, ProcessPoint))
		{
			if (Context->bComputeEdgeType) { Context->SetState(PCGExGraph::State_FindingEdgeTypes); }
			else { Context->SetState(PCGExGraph::State_ReadyForNextGraph); }
		}
	}

	// Process params again for edges types
	auto ProcessPointForGraph = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
	{
		PCGExGraph::ComputeEdgeType(Context->SocketInfos, PointIO->GetOutPoint(PointIndex), PointIndex, PointIO);
	};

	if (Context->IsState(PCGExGraph::State_FindingEdgeTypes))
	{
		if (Context->AsyncProcessingCurrentPoints(ProcessPointForGraph))
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

#undef LOCTEXT_NAMESPACE
