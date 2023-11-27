// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExPartitionGraphPatches.h"

#include "PCGPin.h"

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
		Context->SetState(PCGExMT::EState::ReadyForNextGraph);
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph(true))
		{
			Context->SetState(PCGExMT::EState::Done); //No more params
		}
		else
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
		}
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO(false))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextGraph); //No more points, move to next params
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingPoints);
		}
	}

	// 1st Pass on points

	auto InitializePointsInput = [&](const UPCGExPointIO* PointIO)
	{
		Context->PreparePatchGroup();
		Context->PrepareCurrentGraphForPoints(PointIO->In, false); // Prepare to read PointIO->In
	};

	auto ProcessPoint = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
	{
		Context->Patches->Distribute(PointIndex);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->AsyncProcessingCurrentPoints(InitializePointsInput, ProcessPoint))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
			Context->Patches->OutputTo(Context, Context->MinPatchSize, Context->MaxPatchSize);
		}
	}

	// Done

	if (Context->IsState(PCGExMT::EState::Done))
	{
		Context->OutputGraphParams();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
