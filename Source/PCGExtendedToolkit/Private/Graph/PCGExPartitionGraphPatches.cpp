// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExPartitionGraphPatches.h"

#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "PCGPin.h"
#include "Graph/PCGExGraphHelpers.h"
#include "Graph/PCGExGraphPatch.h"

#define LOCTEXT_NAMESPACE "PCGExPartitionGraphPatches"

int32 UPCGExPartitionGraphPatchesSettings::GetPreferredChunkSize() const { return 32; }

PCGEx::EIOInit UPCGExPartitionGraphPatchesSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::NoOutput; }

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

	auto InitializePointsInput = [&Context](UPCGExPointIO* IO)
	{
		if (Context->Patches)
		{
			//TODO: Clear last patches
		}
		Context->PreparePatchGroup();
		Context->CurrentGraph->PrepareForPointData(Context, IO->In, false); // Prepare to read IO->In
	};

	auto ProcessPoint = [&Context](
		const FPCGPoint& Point, const int32 ReadIndex, const UPCGExPointIO* IO)
	{
		if (Context->Patches->Contains(ReadIndex)) { return; } // This point has already been processed.
		FWriteScopeLock ScopeLock(Context->ContextLock);
		Context->Patches->Distribute(Point, ReadIndex);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->CurrentIO->InputParallelProcessing(Context, InitializePointsInput, ProcessPoint, 32))
		{
			UE_LOG(LogTemp, Warning, TEXT("Found %d patches."), Context->Patches->Patches.Num());
			for (UPCGExGraphPatch* Patch : Context->Patches->Patches)
			{
			}
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
			Context->Patches->OutputTo(Context, Context->MinPatchSize, Context->MaxPatchSize);
		}
	}

	// Done

	if (Context->IsState(PCGExMT::EState::Done))
	{
		if (Context->Patches)
		{
			//TODO: Clear last patches
		}
		Context->OutputParams();
		return true;
	}

	return false;
}

/**
 * Recursively register connected neighbors
 * @param Context 
 * @param Neighbors 
 * @param Index 
 */
void FindNeighbors(FPCGExPartitionGraphPatchesContext* Context, TArray<int32>& Neighbors, const int32 Index)
{
	if (Neighbors.Contains(Index)) { return; }

	FPCGPoint Point = Context->CurrentIO->In->GetPoint(Index);

	TArray<PCGExGraph::FSocketMetadata> SocketMetadatas;
	Context->CurrentGraph->GetSocketsData(Point.MetadataEntry, SocketMetadatas);

	TArray<PCGExGraph::FSocketMetadata> LocalNeighbors;
	for (const PCGExGraph::FSocketMetadata& Data : SocketMetadatas)
	{
		if (static_cast<uint8>((Data.EdgeType & static_cast<EPCGExEdgeType>(Context->CrawlEdgeTypes))) == 0) { continue; }
		LocalNeighbors.AddUnique(Data);
	}

	if (LocalNeighbors.IsEmpty()) { return; }

	Neighbors.AddUnique(Index); //Register current index
	for (const PCGExGraph::FSocketMetadata& Data : LocalNeighbors)
	{
		FindNeighbors(Context, Neighbors, Data.Index);
	}
}

#undef LOCTEXT_NAMESPACE
