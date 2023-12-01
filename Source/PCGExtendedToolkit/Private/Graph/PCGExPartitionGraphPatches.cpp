// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExPartitionGraphPatches.h"

#include "Graph/PCGExGraphPatch.h"

#define LOCTEXT_NAMESPACE "PCGExPartitionGraphPatches"

int32 UPCGExPartitionGraphPatchesSettings::GetPreferredChunkSize() const { return 32; }

PCGExIO::EInitMode UPCGExPartitionGraphPatchesSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::NoOutput; }

TArray<FPCGPinProperties> UPCGExPartitionGraphPatchesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	FPCGPinProperties& ParamsInputPin = PinProperties.Last();
	ParamsInputPin.bAllowMultipleConnections = false;
	ParamsInputPin.bAllowMultipleData = false;
	return PinProperties;
}

FPCGElementPtr UPCGExPartitionGraphPatchesSettings::CreateElement() const
{
	return MakeShared<FPCGExPartitionGraphPatchesElement>();
}

FPCGContext* FPCGExPartitionGraphPatchesElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExPartitionGraphPatchesContext* Context = new FPCGExPartitionGraphPatchesContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExPartitionGraphPatchesSettings* Settings = Context->GetInputSettings<UPCGExPartitionGraphPatchesSettings>();
	check(Settings);

	Context->CrawlEdgeTypes = static_cast<EPCGExEdgeType>(Settings->CrawlEdgeTypes);
	Context->bRemoveSmallPatches = Settings->bRemoveSmallPatches;
	Context->MinPatchSize = Settings->bRemoveSmallPatches ? Settings->MinPatchSize : -1;
	Context->bRemoveBigPatches = Settings->bRemoveBigPatches;
	Context->MaxPatchSize = Settings->bRemoveBigPatches ? Settings->MaxPatchSize : -1;
	Context->PatchIDAttributeName = Settings->PatchIDAttributeName;
	Context->PatchSizeAttributeName = Settings->PatchSizeAttributeName;
	Context->ResolveRoamingMethod = Settings->ResolveRoamingMethod;


	return Context;
}

bool FPCGExPartitionGraphPatchesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPartitionGraphPatchesElement::Execute);

	FPCGExPartitionGraphPatchesContext* Context = static_cast<FPCGExPartitionGraphPatchesContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
		Context->SetState(PCGExGraph::State_ReadyForNextGraph);
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph(true)) { Context->Done(); }
		else { Context->SetState(PCGExMT::State_ReadyForNextPoints); }
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO(false))
		{
			Context->SetState(PCGExGraph::State_ReadyForNextGraph); //No more points, move to next params
		}
		else
		{
			Context->SetState(PCGExGraph::State_FindingPatch);
		}
	}

	// 1st Pass on points


	if (Context->IsState(PCGExGraph::State_FindingPatch))
	{
		auto Initialize = [&](const UPCGExPointIO* PointIO)
		{
			Context->PreparePatchGroup(); 
			Context->PrepareCurrentGraphForPoints(PointIO->In, false); // Prepare to read PointIO->In
		};

		auto ProcessPoint = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
		{
			//Context->Patches->Distribute(PointIndex);
			Context->CreateAndStartTask<FPatchTask>(PointIndex, PointIO->GetInPoint(PointIndex).MetadataEntry, 0);
		};

		if (Context->AsyncProcessingCurrentPoints(Initialize, ProcessPoint))
		{
			Context->SetState(PCGExMT::State_WaitingOnAsyncWork);
			//Context->SetState(PCGExMT::State_ReadyForNextPoints);
			//Context->Patches->OutputTo(Context, Context->MinPatchSize, Context->MaxPatchSize);
		}
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (Context->IsAsyncWorkComplete())
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			Context->Patches->OutputTo(Context, Context->MinPatchSize, Context->MaxPatchSize);
		}
	}

	if (Context->IsDone())
	{
		Context->OutputGraphParams();
		return true;
	}

	return false;
}

void FPatchTask::ExecuteTask()
{
	const FPCGExPartitionGraphPatchesContext* Context = static_cast<FPCGExPartitionGraphPatchesContext*>(TaskContext);
	Context->Patches->Distribute(Infos.Index);
	ExecutionComplete(true);
}

#undef LOCTEXT_NAMESPACE
