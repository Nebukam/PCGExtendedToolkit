// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFindEdgePatches.h"

#include "Data/PCGExData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE FindEdgePatches

namespace PCGExGraph
{
	FPatch::~FPatch()
	{
		IndicesSet.Empty();
		EdgesHashSet.Empty();
		PointIO = nullptr;
		Parent = nullptr;
	}

	void FPatch::Add(const int32 InIndex)
	{
		{
			FReadScopeLock ReadLock(HashLock);
			if (IndicesSet.Contains(InIndex)) { return; }
		}

		{
			FWriteScopeLock WriteLock(HashLock);
			IndicesSet.Add(InIndex);
		}
		{
			FWriteScopeLock WriteLock(Parent->HashLock);
			Parent->IndicesMap.Add(InIndex, this);
		}
	}

	bool FPatch::Contains(const int32 InIndex) const
	{
		FReadScopeLock ReadLock(HashLock);
		return IndicesSet.Contains(InIndex);
	}

	void FPatch::AddEdge(uint64 InEdgeHash)
	{
		const PCGExGraph::FEdge Edge = static_cast<PCGExGraph::FEdge>(InEdgeHash);
		Add(Edge.Start);
		Add(Edge.End);
		{
			FWriteScopeLock WriteLock(HashLock);
			EdgesHashSet.Add(InEdgeHash);
		}
	}

	bool FPatch::ContainsEdge(const uint64 InEdgeHash) const
	{
		FReadScopeLock ReadLock(HashLock);
		return EdgesHashSet.Contains(InEdgeHash);
	}

	bool FPatch::OutputTo(const PCGExData::FPointIO& OutIO, int32 PatchIDOverride)
	{
		const TArray<FPCGPoint>& InPoints = OutIO.GetIn()->GetPoints();
		UPCGPointData* OutData = OutIO.GetOut();
		TArray<FPCGPoint>& Points = OutData->GetMutablePoints();

		Points.Reserve(Points.Num() + IndicesSet.Num());
		PCGMetadataElementCommon::ClearOrCreateAttribute(OutData->Metadata, Parent->PatchIDAttributeName, PatchIDOverride < 0 ? PatchID : PatchIDOverride);
		PCGMetadataElementCommon::ClearOrCreateAttribute(OutData->Metadata, Parent->PatchSizeAttributeName, IndicesSet.Num());
		for (const int32 Index : IndicesSet)
		{
			FPCGPoint& NewPoint = Points.Emplace_GetRef(InPoints[Index]);
		}
		return true;
	}

	bool FPatchGroup::Contains(const uint64 Hash) const
	{
		FReadScopeLock ReadLock(HashLock);
		return IndicesMap.Contains(Hash);
	}

	FPatch* FPatchGroup::FindPatch(uint64 Hash)
	{
		FReadScopeLock ReadLock(HashLock);

		FPatch** PatchPtr = IndicesMap.Find(Hash);
		if (!PatchPtr) { return nullptr; }

		return *PatchPtr;
	}

	FPatch* FPatchGroup::GetOrCreatePatch(const uint64 Hash)
	{
		{
			FReadScopeLock ReadLock(HashLock);
			if (FPatch** PatchPtr = IndicesMap.Find(Hash)) { return *PatchPtr; }
		}

		FPatch* NewPatch = CreatePatch();
		NewPatch->Add(Hash);
		return NewPatch;
	}

	FPatch* FPatchGroup::CreatePatch()
	{
		FWriteScopeLock WriteLock(PatchesLock);
		FPatch* NewPatch = new FPatch();
		Patches.Add(NewPatch);
		NewPatch->Parent = this;
		NewPatch->PointIO = PointIO;
		NewPatch->PatchID = Patches.Num() - 1;
		return NewPatch;
	}

	FPatch* FPatchGroup::Distribute(const int32 InIndex, FPatch* Patch)
	{
		if (Patch)
		{
			if (Patch->Contains(InIndex))
			{
				// Exit early since this point index has already been registered in this patch.
				return Patch;
			}

			// Otherwise add index to active patch
			Patch->Add(InIndex);
		}

		TArray<PCGExGraph::FUnsignedEdge> UnsignedEdges;
		UnsignedEdges.Reserve(NumMaxEdges);

		CurrentGraph->GetEdges(InIndex, UnsignedEdges, CrawlEdgeTypes);

		for (const PCGExGraph::FUnsignedEdge& UEdge : UnsignedEdges)
		{
			if (!Patch) { Patch = GetOrCreatePatch(InIndex); }
			Distribute(UEdge.End, Patch);
			Patch->AddEdge(UEdge.GetUnsignedHash());
		}

		return Patch;
	}

	void FPatchGroup::OutputTo(FPCGContext* Context)
	{
		PatchesIO = new PCGExData::FPointIOGroup();
		for (FPatch* Patch : Patches)
		{
			PCGExData::FPointIO& OutIO = PatchesIO->Emplace_GetRef(*PointIO, PCGExData::EInit::NewOutput);
			Patch->OutputTo(OutIO, -1);
		}
		PatchesIO->OutputTo(Context);
		PCGEX_DELETE(PatchesIO)
	}

	FPatchGroup::~FPatchGroup()
	{
		for (const FPatch* Patch : Patches) { delete Patch; }

		Patches.Empty();
		IndicesMap.Empty();
		PointIO = nullptr;
		CurrentGraph = nullptr;
		PatchesIO = nullptr;
	}

	void FPatchGroup::OutputTo(FPCGContext* Context, const int64 MinPointCount, const int64 MaxPointCount, const uint32 PUID)
	{
		PatchesIO = new PCGExData::FPointIOGroup();
		int32 PatchIndex = 0;
		for (FPatch* Patch : Patches)
		{
			const int64 OutNumPoints = Patch->IndicesSet.Num();
			if (MinPointCount >= 0 && OutNumPoints < MinPointCount) { continue; }
			if (MaxPointCount >= 0 && OutNumPoints > MaxPointCount) { continue; }
			PCGExData::FPointIO& OutIO = PatchesIO->Emplace_GetRef(*PointIO, PCGExData::EInit::NewOutput);
			OutIO.GetOut()->Metadata->CreateAttribute<int32>(PCGExGraph::PUIDAttributeName, PUID, false, true);
			Patch->OutputTo(OutIO, PatchIndex);
			PatchIndex++;
		}
		PatchesIO->OutputTo(Context);
		PCGEX_DELETE(PatchesIO)
	}
}

int32 UPCGExFindEdgePatchesSettings::GetPreferredChunkSize() const { return 32; }

PCGExData::EInit UPCGExFindEdgePatchesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGExFindEdgePatchesContext::~FPCGExFindEdgePatchesContext()
{
	PCGEX_CLEANUP_ASYNC

	PCGEX_DELETE(PatchesIO);
	PCGEX_DELETE(Patches);
}

TArray<FPCGPinProperties> UPCGExFindEdgePatchesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinPatchesOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPatchesOutput.Tooltip = FTEXT("Point data representing edges.");
#endif // WITH_EDITOR

	PCGEx::Swap(PinProperties, PinProperties.Num() - 1, PinProperties.Num() - 2);
	return PinProperties;
}

FPCGElementPtr UPCGExFindEdgePatchesSettings::CreateElement() const
{
	return MakeShared<FPCGExFindEdgePatchesElement>();
}

PCGEX_INITIALIZE_CONTEXT(FindEdgePatches)

bool FPCGExFindEdgePatchesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExGraphProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindEdgePatches)

	Context->CrawlEdgeTypes = static_cast<EPCGExEdgeType>(Settings->CrawlEdgeTypes);

	PCGEX_FWD(bRemoveSmallPatches)
	Context->MinPatchSize = Settings->bRemoveSmallPatches ? Settings->MinPatchSize : -1;

	PCGEX_FWD(bRemoveBigPatches)
	Context->MaxPatchSize = Settings->bRemoveBigPatches ? Settings->MaxPatchSize : -1;

	PCGEX_FWD(PatchIDAttributeName)
	PCGEX_FWD(PatchSizeAttributeName)
	PCGEX_FWD(ResolveRoamingMethod)

	PCGEX_VALIDATE_NAME(Context->PatchIDAttributeName)
	PCGEX_VALIDATE_NAME(Context->PatchSizeAttributeName)

	return true;
}

bool FPCGExFindEdgePatchesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindEdgePatchesElement::Execute);

	PCGEX_CONTEXT(FindEdgePatches)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
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
		auto Initialize = [&](PCGExData::FPointIO& PointIO)
		{
			Context->PrepareCurrentGraphForPoints(PointIO); // Prepare to read PointIO->In
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			Context->GetAsyncManager()->Start<FDistributeToPatchTask>(PointIndex, nullptr);
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint)) { Context->SetAsyncState(PCGExGraph::State_WaitingOnFindingPatch); }
	}

	if (Context->IsState(PCGExGraph::State_WaitingOnFindingPatch))
	{
		if (Context->IsAsyncWorkComplete())
		{
			if()
			Context->SetState(PCGExGraph::State_ReadyForNextGraph);
		}
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

		for (PCGExGraph::FPatch* Patch : Context->Patches->Patches)
		{
			const int64 OutNumPoints = Patch->IndicesSet.Num();
			if (Context->MinPatchSize >= 0 && OutNumPoints < Context->MinPatchSize) { continue; }
			if (Context->MaxPatchSize >= 0 && OutNumPoints > Context->MaxPatchSize) { continue; }

			// Create and mark patch data
			UPCGPointData* PatchData = PCGExData::PCGExPointIO::NewEmptyPointData(Context, PCGExGraph::OutputEdgesLabel);
			PCGExData::WriteMark<int64>(PatchData->Metadata, PCGExGraph::PUIDAttributeName, PUID);

			// Mark point data
			PCGExData::WriteMark<int64>(Context->CurrentIO->GetOut()->Metadata, PCGExGraph::PUIDAttributeName, PUID);

			Context->GetAsyncManager()->Start<FWritePatchesTask>(Context->PatchUIndex, Context->CurrentIO, Patch, PatchData);

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
	FPCGExFindEdgePatchesContext* Context = Manager->GetContext<FPCGExFindEdgePatchesContext>();
	PCGEX_ASYNC_CHECKPOINT

	{
		FReadScopeLock ReadLock(Context->QueueLock);
		if (!Context->IndicesQueue.Contains(TaskInfos.Index)) { return false; }
	}

	FWriteScopeLock WriteLock(Context->QueueLock);
	PCGExGraph::FPatch* Patch = Context->Patches->Distribute(TaskInfos.Index);

	for (const int32 Index : Patch->IndicesSet) { Context->IndicesQueue.Remove(Index); }

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

	const int32 NumPoints = Patch->IndicesSet.Num();

	UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;
	PCGExData::WriteMark<int64>(Metadata, Context->Patches->PatchIDAttributeName, TaskInfos.Index);
	PCGExData::WriteMark<int64>(Metadata, Context->Patches->PatchSizeAttributeName, NumPoints);

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
#undef PCGEX_NAMESPACE
