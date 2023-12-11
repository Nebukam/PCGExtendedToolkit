// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFindEdgePatches.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Graph/PCGExGraphPatch.h"

#define LOCTEXT_NAMESPACE "PCGExFindEdgePatches"

int32 UPCGExFindEdgePatchesSettings::GetPreferredChunkSize() const { return 32; }

PCGExData::EInit UPCGExFindEdgePatchesSettings::GetPointOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGExFindEdgePatchesContext::~FPCGExFindEdgePatchesContext()
{
	PCGEX_DELETE(PatchesIO);
	PCGEX_DELETE(Patches);
}

TArray<FPCGPinProperties> UPCGExFindEdgePatchesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinPatchesOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPatchesOutput.Tooltip = LOCTEXT("PCGExOutputParamsTooltip", "Point data representing edges.");
#endif // WITH_EDITOR

	PCGEx::Swap(PinProperties, PinProperties.Num() - 1, PinProperties.Num() - 2);
	return PinProperties;
}

FPCGElementPtr UPCGExFindEdgePatchesSettings::CreateElement() const
{
	return MakeShared<FPCGExFindEdgePatchesElement>();
}

FPCGContext* FPCGExFindEdgePatchesElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExFindEdgePatchesContext* Context = new FPCGExFindEdgePatchesContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	PCGEX_SETTINGS(UPCGExFindEdgePatchesSettings)

	Context->CrawlEdgeTypes = static_cast<EPCGExEdgeType>(Settings->CrawlEdgeTypes);

	PCGEX_FWD(bRemoveSmallPatches)
	Context->MinPatchSize = Settings->bRemoveSmallPatches ? Settings->MinPatchSize : -1;

	PCGEX_FWD(bRemoveBigPatches)
	Context->MaxPatchSize = Settings->bRemoveBigPatches ? Settings->MaxPatchSize : -1;

	PCGEX_FWD(PatchIDAttributeName)
	PCGEX_FWD(PatchSizeAttributeName)
	PCGEX_FWD(ResolveRoamingMethod)

	return Context;
}


bool FPCGExFindEdgePatchesElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExGraphProcessorElement::Validate(InContext)) { return false; }

	PCGEX_CONTEXT(FPCGExFindEdgePatchesContext)

	PCGEX_VALIDATE_NAME(Context->PatchIDAttributeName)
	PCGEX_VALIDATE_NAME(Context->PatchSizeAttributeName)
	
	return true;
}

bool FPCGExFindEdgePatchesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindEdgePatchesElement::Execute);

	FPCGExFindEdgePatchesContext* Context = static_cast<FPCGExFindEdgePatchesContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
		Context->PatchesIO = new PCGExData::FPointIOGroup();
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO(true)) { Context->Done(); }
		else
		{
			Context->PreparePatchGroup(); // Prepare patch group for current points
			Context->SetState(PCGExGraph::State_ReadyForNextGraph);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph())
		{
			// No more graph for the current points, start matching patches.
			Context->SetState(PCGExGraph::State_MergingPatch);
		}
		else
		{
			Context->UpdatePatchGroup();
			Context->SetState(PCGExGraph::State_FindingPatch);
		}
	}

	// -> Process current points with current graph

	if (Context->IsState(PCGExGraph::State_FindingPatch))
	{
		auto Initialize = [&](const PCGExData::FPointIO& PointIO)
		{
			Context->PrepareCurrentGraphForPoints(PointIO.GetIn(), false); // Prepare to read PointIO->In
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			Context->GetAsyncManager()->Start<FDistributeToPatchTask>(PointIndex, PointIO.GetInPoint(PointIndex).MetadataEntry, nullptr);
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint)) { Context->SetAsyncState(PCGExGraph::State_WaitingOnFindingPatch); }
	}

	if (Context->IsState(PCGExGraph::State_WaitingOnFindingPatch))
	{
		if (Context->IsAsyncWorkComplete()) { Context->SetState(PCGExGraph::State_ReadyForNextGraph); }
	}

	// -> Each graph has been traversed, now merge patches

	if (Context->IsState(PCGExGraph::State_MergingPatch))
	{
		// TODO: Start FConsolidatePatchesTask
		Context->SetAsyncState(PCGExGraph::State_WaitingOnMergingPatch);
	}

	if (Context->IsState(PCGExGraph::State_WaitingOnMergingPatch))
	{
		if (Context->IsAsyncWorkComplete()) { Context->SetState(PCGExGraph::State_WritingPatch); }
	}

	// -> Patches have been merged, now write patches

	if (Context->IsState(PCGExGraph::State_WritingPatch))
	{
		// TODO: Start FWritePatchesTask

		int32 PUID = Context->CurrentIO->GetIn()->GetUniqueID();

		for (FPCGExGraphPatch* Patch : Context->Patches->Patches)
		{
			const int64 OutNumPoints = Patch->IndicesSet.Num();
			if (Context->MinPatchSize >= 0 && OutNumPoints < Context->MinPatchSize) { continue; }
			if (Context->MaxPatchSize >= 0 && OutNumPoints > Context->MaxPatchSize) { continue; }

			// Create and mark patch data
			UPCGPointData* PatchData = PCGExData::PCGExPointIO::NewEmptyPointData(Context, PCGExGraph::OutputEdgesLabel);
			PCGEx::CreateMark(PatchData->Metadata, PCGExGraph::PUIDAttributeName, PUID);

			// Mark point data
			PCGEx::CreateMark(Context->CurrentIO->GetOut()->Metadata, PCGExGraph::PUIDAttributeName, PUID);

			Context->GetAsyncManager()->Start<FWritePatchesTask>(Context->PatchUIndex, -1, Context->CurrentIO, Patch, PatchData);

			Context->PatchUIndex++;
		}

		Context->SetAsyncState(PCGExGraph::State_WaitingOnWritingPatch);
	}

	if (Context->IsState(PCGExGraph::State_WaitingOnWritingPatch))
	{
		if (Context->IsAsyncWorkComplete())
		{
			PCGEX_DELETE(Context->Patches)
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	if (Context->IsDone())
	{
		Context->OutputPointsAndGraphParams();
	}

	return Context->IsDone();
}

bool FDistributeToPatchTask::ExecuteTask()
{
	const FPCGExFindEdgePatchesContext* Context = Manager->GetContext<FPCGExFindEdgePatchesContext>();
	PCGEX_ASYNC_CHECKPOINT

	Context->Patches->Distribute(TaskInfos.Index);

	return true;
}

bool FConsolidatePatchesTask::ExecuteTask()
{
	PCGEX_ASYNC_CHECKPOINT

	// TODO : Check if multiple patches overlap, and merge them
	return true;
}

bool FWritePatchesTask::ExecuteTask()
{
	FPCGExFindEdgePatchesContext* Context = Manager->GetContext<FPCGExFindEdgePatchesContext>();
	PCGEX_ASYNC_CHECKPOINT

	const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
	TArray<FPCGPoint>& MutablePoints = PatchData->GetMutablePoints();
	MutablePoints.Reserve(MutablePoints.Num() + Patch->IndicesSet.Num());

	int32 NumPoints = Patch->IndicesSet.Num();

	UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;
	PCGEx::CreateMark(Metadata, Context->Patches->PatchIDAttributeName, TaskInfos.Index);
	PCGEx::CreateMark(Metadata, Context->Patches->PatchSizeAttributeName, NumPoints);

	FPCGMetadataAttribute<int32>* StartIndexAttribute = PatchData->Metadata->FindOrCreateAttribute<int32>(FName("StartIndex"), -1);
	FPCGMetadataAttribute<int32>* EndIndexAttribute = PatchData->Metadata->FindOrCreateAttribute<int32>(FName("EndIndex"), -1);

	for (const uint64 Hash : Patch->IndicesSet)
	{
		PCGEX_ASYNC_CHECKPOINT

		FPCGPoint& NewPoint = MutablePoints.Emplace_GetRef();
		PatchData->Metadata->InitializeOnSet(NewPoint.MetadataEntry);

		PCGExGraph::FUnsignedEdge Edge = static_cast<PCGExGraph::FUnsignedEdge>(Hash);
		StartIndexAttribute->SetValue(NewPoint.MetadataEntry, Edge.Start);
		EndIndexAttribute->SetValue(NewPoint.MetadataEntry, Edge.End);
	}


	return true;
}

#undef LOCTEXT_NAMESPACE
