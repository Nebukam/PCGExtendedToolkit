// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExEdgesProcessor.h"


#include "Graph/PCGExClusterMT.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface


PCGExData::EInit UPCGExEdgesProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EInit::Forward; }

PCGExData::EInit UPCGExEdgesProcessorSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::Forward; }

bool UPCGExEdgesProcessorSettings::GetMainAcceptMultipleData() const { return true; }

TArray<FPCGPinProperties> UPCGExEdgesProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::SourceEdgesLabel, "Edges associated with the main input points", Required, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExEdgesProcessorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Edges associated with the main output points", Required, {})
	return PinProperties;
}

#pragma endregion

FPCGExEdgesProcessorContext::~FPCGExEdgesProcessorContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(InputDictionary)
	PCGEX_DELETE(MainEdges)
	PCGEX_DELETE(CurrentCluster)

	PCGEX_DELETE_TARRAY(Batches)

	EndpointsLookup.Empty();
}

bool FPCGExEdgesProcessorContext::AdvancePointsIO(const bool bCleanupKeys)
{
	PCGEX_DELETE(CurrentCluster)

	CurrentEdgesIndex = -1;
	EndpointsLookup.Empty();
	EndpointsAdjacency.Empty();

	if (!FPCGExPointsProcessorContext::AdvancePointsIO(bCleanupKeys)) { return false; }

	if (FString CurrentPairId;
		CurrentIO->Tags->GetValue(PCGExGraph::TagStr_ClusterPair, CurrentPairId))
	{
		FString OutId;
		PCGExGraph::SetClusterVtx(CurrentIO, OutId);

		TaggedEdges = InputDictionary->GetEntries(CurrentPairId);
		if (TaggedEdges && !TaggedEdges->Entries.IsEmpty()) { PCGExGraph::MarkClusterEdges(TaggedEdges->Entries, OutId); }
		else { TaggedEdges = nullptr; }
	}
	else { TaggedEdges = nullptr; }

	if (TaggedEdges)
	{
		CurrentIO->CreateInKeys();
		//ProjectionSettings.Init(CurrentIO); // TODO : Move to FClusterProcessor?
		if (bBuildEndpointsLookup) { PCGExGraph::BuildEndpointsLookup(CurrentIO, EndpointsLookup, EndpointsAdjacency); }
	}
	else
	{
		PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Some input vtx have no associated edges."));
	}

	return true;
}

bool FPCGExEdgesProcessorContext::AdvanceEdges(const bool bBuildCluster, const bool bCleanupKeys)
{
	PCGEX_DELETE(CurrentCluster)

	if (bCleanupKeys && CurrentEdges) { CurrentEdges->CleanupKeys(); }

	if (TaggedEdges && TaggedEdges->Entries.IsValidIndex(++CurrentEdgesIndex))
	{
		CurrentEdges = TaggedEdges->Entries[CurrentEdgesIndex];

		if (!bBuildCluster) { return true; }

		CurrentEdges->CreateInKeys();

		if (const PCGExCluster::FCluster* CachedCluster = PCGExClusterData::TryGetCachedCluster(CurrentIO, CurrentEdges))
		{
			CurrentCluster = new PCGExCluster::FCluster(
				CachedCluster, CurrentIO, CurrentEdges,
				false, false, false);
		}

		if (!CurrentCluster)
		{
			CurrentCluster = new PCGExCluster::FCluster();
			CurrentCluster->bIsOneToOne = (TaggedEdges->Entries.Num() == 1);

			if (!CurrentCluster->BuildFrom(
				CurrentEdges, CurrentIO->GetIn()->GetPoints(),
				EndpointsLookup, &EndpointsAdjacency))
			{
				PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Some clusters are corrupted and will not be processed.  If you modified vtx/edges manually, make sure to use Sanitize Clusters first."));
				PCGEX_DELETE(CurrentCluster)
			}
			else
			{
				CurrentCluster->VtxIO = CurrentIO;
				CurrentCluster->EdgesIO = CurrentEdges;
			}
		}

		return true;
	}

	CurrentEdges = nullptr;
	return false;
}

bool FPCGExEdgesProcessorContext::ProcessClusters()
{
	if (Batches.IsEmpty()) { return true; }

	if (bClusterBatchInlined)
	{
		if (!CurrentBatch) { return true; }

		if (IsState(PCGExClusterMT::MTState_ClusterProcessing))
		{
			if (!IsAsyncWorkComplete()) { return false; }

			CompleteBatch(GetAsyncManager(), CurrentBatch);
			SetAsyncState(PCGExClusterMT::MTState_ClusterCompletingWork);
		}

		if (IsState(PCGExClusterMT::MTState_ClusterCompletingWork))
		{
			if (!IsAsyncWorkComplete()) { return false; }

			// TODO : This is a mess, look into it

			if (!bDoClusterBatchGraphBuilding) { AdvanceBatch(); }
			else
			{
				bClusterBatchInlined = false;
				for (const PCGExClusterMT::FClusterProcessorBatchBase* Batch : Batches)
				{
					Batch->GraphBuilder->CompileAsync(GetAsyncManager());
				}
				SetAsyncState(PCGExGraph::State_Compiling);
			}
		}
	}
	else
	{
		if (IsState(PCGExClusterMT::MTState_ClusterProcessing))
		{
			if (!IsAsyncWorkComplete()) { return false; }

			OnBatchesProcessingDone();
			CompleteBatches(GetAsyncManager(), Batches);
			SetAsyncState(PCGExClusterMT::MTState_ClusterCompletingWork);
		}

		if (IsState(PCGExClusterMT::MTState_ClusterCompletingWork))
		{
			if (!IsAsyncWorkComplete()) { return false; }

			OnBatchesCompletingWorkDone();

			if (!bDoClusterBatchGraphBuilding)
			{
				if (bDoClusterBatchWritingStep)
				{
					WriteBatches(GetAsyncManager(), Batches);
					SetAsyncState(PCGExClusterMT::MTState_ClusterWriting);
				}
				else { SetState(TargetState_ClusterProcessingDone); }
			}
			else
			{
				for (const PCGExClusterMT::FClusterProcessorBatchBase* Batch : Batches)
				{
					Batch->GraphBuilder->CompileAsync(GetAsyncManager());
				}
				SetAsyncState(PCGExGraph::State_Compiling);
			}
		}

		if (IsState(PCGExGraph::State_Compiling))
		{
			if (!IsAsyncWorkComplete()) { return false; }

			OnBatchesCompilationDone(false);

			for (const PCGExClusterMT::FClusterProcessorBatchBase* Batch : Batches)
			{
				if (Batch->GraphBuilder->bCompiledSuccessfully) { Batch->GraphBuilder->Write(); }
			}

			OnBatchesCompilationDone(true);

			if (bDoClusterBatchWritingStep)
			{
				WriteBatches(GetAsyncManager(), Batches);
				SetAsyncState(PCGExClusterMT::MTState_ClusterWriting);
			}
			else { SetState(TargetState_ClusterProcessingDone); }
		}

		if (IsState(PCGExClusterMT::MTState_ClusterWriting))
		{
			if (!IsAsyncWorkComplete()) { return false; }
			OnBatchesWritingDone();
			SetState(TargetState_ClusterProcessingDone);
		}
	}

	return true;
}

bool FPCGExEdgesProcessorContext::HasValidHeuristics() const
{
	TArray<UPCGExParamFactoryBase*> InputFactories;
	const bool bFoundAny = PCGExFactories::GetInputFactories(
		this, PCGExGraph::SourceHeuristicsLabel, InputFactories,
		{PCGExFactories::EType::Heuristics}, false);
	InputFactories.Empty();
	return bFoundAny;
}

void FPCGExEdgesProcessorContext::AdvanceBatch()
{
	CurrentBatchIndex++;
	if (!Batches.IsValidIndex(CurrentBatchIndex))
	{
		CurrentBatch = nullptr;
		SetState(TargetState_ClusterProcessingDone);
	}
	else
	{
		CurrentBatch = Batches[CurrentBatchIndex];
		ScheduleBatch(GetAsyncManager(), CurrentBatch);
		SetAsyncState(PCGExClusterMT::MTState_ClusterProcessing);
	}
}

void FPCGExEdgesProcessorContext::OutputPointsAndEdges() const
{
	MainPoints->OutputToContext();
	MainEdges->OutputToContext();
}

PCGEX_INITIALIZE_CONTEXT(EdgesProcessor)

void FPCGExEdgesProcessorElement::DisabledPassThroughData(FPCGContext* Context) const
{
	FPCGExPointsProcessorElement::DisabledPassThroughData(Context);

	//Forward main edges
	TArray<FPCGTaggedData> EdgesSources = Context->InputData.GetInputsByPin(PCGExGraph::SourceEdgesLabel);
	for (const FPCGTaggedData& TaggedData : EdgesSources)
	{
		FPCGTaggedData& TaggedDataCopy = Context->OutputData.TaggedData.Emplace_GetRef();
		TaggedDataCopy.Data = TaggedData.Data;
		TaggedDataCopy.Tags.Append(TaggedData.Tags);
		TaggedDataCopy.Pin = PCGExGraph::OutputEdgesLabel;
	}
}

bool FPCGExEdgesProcessorElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(EdgesProcessor)

	Context->bHasValidHeuristics = Context->HasValidHeuristics();

	if (Context->MainEdges->IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Edges."));
		return false;
	}

	return true;
}

FPCGContext* FPCGExEdgesProcessorElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExPointsProcessorElement::InitializeContext(InContext, InputData, SourceComponent, Node);

	PCGEX_CONTEXT_AND_SETTINGS(EdgesProcessor)

	if (!Settings->bEnabled) { return Context; }

	Context->InputDictionary = new PCGExData::FPointIOTaggedDictionary(PCGExGraph::TagStr_ClusterPair);

	TArray<PCGExData::FPointIO*> TaggedVtx;
	TArray<PCGExData::FPointIO*> TaggedEdges;

	Context->MainEdges = new PCGExData::FPointIOCollection(Context);
	Context->MainEdges->DefaultOutputLabel = PCGExGraph::OutputEdgesLabel;
	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExGraph::SourceEdgesLabel);
	Context->MainEdges->Initialize(Sources, Settings->GetEdgeOutputInitMode());

	// Gather Vtx inputs
	for (PCGExData::FPointIO* MainIO : Context->MainPoints->Pairs)
	{
		if (MainIO->Tags->RawTags.Contains(PCGExGraph::TagStr_PCGExVtx))
		{
			if (MainIO->Tags->RawTags.Contains(PCGExGraph::TagStr_PCGExEdges))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Uh oh, a data is marked as both Vtx and Edges -- it will be ignored for safety."));
				continue;
			}

			TaggedVtx.Add(MainIO);
			continue;
		}

		if (MainIO->Tags->RawTags.Contains(PCGExGraph::TagStr_PCGExEdges))
		{
			if (MainIO->Tags->RawTags.Contains(PCGExGraph::TagStr_PCGExVtx))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Uh oh, a data is marked as both Vtx and Edges. It will be ignored."));
				continue;
			}

			PCGE_LOG(Warning, GraphAndLog, FTEXT("Uh oh, some Edge data made its way to the vtx input. It will be ignored."));
			continue;
		}

		PCGE_LOG(Warning, GraphAndLog, FTEXT("A data pluggued into Vtx is neither tagged Vtx or Edges and will be ignored."));
	}

	// Gather Edge inputs
	for (PCGExData::FPointIO* MainIO : Context->MainEdges->Pairs)
	{
		if (MainIO->Tags->RawTags.Contains(PCGExGraph::TagStr_PCGExEdges))
		{
			if (MainIO->Tags->RawTags.Contains(PCGExGraph::TagStr_PCGExVtx))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Uh oh, a data is marked as both Vtx and Edges. It will be ignored."));
				continue;
			}

			TaggedEdges.Add(MainIO);
			continue;
		}

		if (MainIO->Tags->RawTags.Contains(PCGExGraph::TagStr_PCGExVtx))
		{
			if (MainIO->Tags->RawTags.Contains(PCGExGraph::TagStr_PCGExEdges))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Uh oh, a data is marked as both Vtx and Edges. It will be ignored."));
				continue;
			}

			PCGE_LOG(Warning, GraphAndLog, FTEXT("Uh oh, some Edge data made its way to the vtx input. It will be ignored."));
			continue;
		}

		PCGE_LOG(Warning, GraphAndLog, FTEXT("A data pluggued into Edges is neither tagged Edges or Vtx and will be ignored."));
	}


	for (PCGExData::FPointIO* Vtx : TaggedVtx)
	{
		if (!PCGExGraph::IsPointDataVtxReady(Vtx->GetIn()->Metadata))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("A Vtx input has no metadata and will be discarded."));
			Vtx->Disable();
			continue;
		}

		if (!Context->InputDictionary->CreateKey(*Vtx))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("At least two Vtx inputs share the same PCGEx/Cluster tag. Only one will be processed."));
			Vtx->Disable();
		}
	}

	for (PCGExData::FPointIO* Edges : TaggedEdges)
	{
		if (!PCGExGraph::IsPointDataEdgeReady(Edges->GetIn()->Metadata))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("An Edges input has no edge metadata and will be discarded."));
			Edges->Disable();
			continue;
		}

		if (!Context->InputDictionary->TryAddEntry(*Edges))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input edges have no associated vtx."));
		}
	}

	return Context;
}

#undef LOCTEXT_NAMESPACE
