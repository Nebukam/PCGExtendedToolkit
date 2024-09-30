// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExEdgesProcessor.h"


#include "Graph/PCGExClusterMT.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface


PCGExData::EInit UPCGExEdgesProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EInit::Forward; }

PCGExData::EInit UPCGExEdgesProcessorSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::Forward; }

bool UPCGExEdgesProcessorSettings::GetMainAcceptMultipleData() const { return true; }

FPCGExEdgesProcessorContext::~FPCGExEdgesProcessorContext()
{
	PCGEX_TERMINATE_ASYNC

	for (const TSharedPtr<PCGExClusterMT::FClusterProcessorBatchBase>& Batch : Batches) { Batch->Cleanup(); }
}

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

bool FPCGExEdgesProcessorContext::AdvancePointsIO(const bool bCleanupKeys)
{
	CurrentCluster.Reset();

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
		//ProjectionSettings.Init(CurrentIO); // TODO : Move to FClusterProcessor?
		if (bBuildEndpointsLookup)
		{
			//check(false) // This node needs to be refactored!
			PCGExGraph::BuildEndpointsLookup(CurrentIO, EndpointsLookup, EndpointsAdjacency);
		}
	}
	else
	{
		PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Some input vtx have no associated edges."));
	}

	return true;
}

bool FPCGExEdgesProcessorContext::AdvanceEdges(const bool bBuildCluster, const bool bCleanupKeys)
{
	CurrentCluster.Reset();

	if (bCleanupKeys && CurrentEdges) { CurrentEdges->CleanupKeys(); }

	if (TaggedEdges && TaggedEdges->Entries.IsValidIndex(++CurrentEdgesIndex))
	{
		CurrentEdges = TaggedEdges->Entries[CurrentEdgesIndex];

		if (!bBuildCluster) { return true; }

		if (const TSharedPtr<PCGExCluster::FCluster> CachedCluster = PCGExClusterData::TryGetCachedCluster(CurrentIO.ToSharedRef(), CurrentEdges.ToSharedRef()))
		{
			CurrentCluster = MakeShared<PCGExCluster::FCluster>(
				CachedCluster.ToSharedRef(), CurrentIO, CurrentEdges,
				false, false, false);
		}

		if (!CurrentCluster)
		{
			CurrentCluster = MakeShared<PCGExCluster::FCluster>(CurrentIO, CurrentEdges);
			CurrentCluster->bIsOneToOne = (TaggedEdges->Entries.Num() == 1);

			if (!CurrentCluster->BuildFrom(EndpointsLookup, &EndpointsAdjacency))
			{
				PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Some clusters are corrupted and will not be processed.  If you modified vtx/edges manually, make sure to use Sanitize Clusters first."));
				CurrentCluster.Reset();
			}
		}

		return true;
	}

	CurrentEdges.Reset();
	return false;
}

void FPCGExEdgesProcessorContext::OutputBatches() const
{
	for (const TSharedPtr<PCGExClusterMT::FClusterProcessorBatchBase>& Batch : Batches) { Batch->Output(); }
}

bool FPCGExEdgesProcessorContext::ProcessClusters(const PCGExMT::AsyncState NextStateId, const bool bIsNextStateAsync)
{
	if (!bBatchProcessingEnabled) { return true; }

	if (bClusterBatchInlined)
	{
		if (!CurrentBatch)
		{
			if (CurrentBatchIndex == -1)
			{
				// First batch
				AdvanceBatch(NextStateId, bIsNextStateAsync);
				return false;
			}
			
			return true;
		}

		if (IsState(PCGExClusterMT::MTState_ClusterProcessing))
		{
			PCGEX_ASYNC_WAIT_INTERNAL

			CurrentBatch->CompleteWork();
			SetAsyncState(PCGExClusterMT::MTState_ClusterCompletingWork);
		}

		if (IsState(PCGExClusterMT::MTState_ClusterCompletingWork))
		{
			PCGEX_ASYNC_WAIT_INTERNAL
			
			AdvanceBatch(NextStateId, bIsNextStateAsync);
		}
	}
	else
	{
		if (IsState(PCGExClusterMT::MTState_ClusterProcessing))
		{
			PCGEX_ASYNC_WAIT_INTERNAL

			ClusterProcessing_InitialProcessingDone();
			CompleteBatches(Batches);
			SetAsyncState(PCGExClusterMT::MTState_ClusterCompletingWork);
		}

		if (IsState(PCGExClusterMT::MTState_ClusterCompletingWork))
		{
			PCGEX_ASYNC_WAIT_INTERNAL

			ClusterProcessing_WorkComplete();

			if (bDoClusterBatchWritingStep)
			{
				WriteBatches(Batches);
				SetAsyncState(PCGExClusterMT::MTState_ClusterWriting);
				return false;
			}

			bBatchProcessingEnabled = false;
			if (NextStateId == PCGExMT::State_Done) { Done(); }
			if (bIsNextStateAsync) { SetAsyncState(NextStateId); }
			else { SetState(NextStateId); }
		}

		if (IsState(PCGExClusterMT::MTState_ClusterWriting))
		{
			PCGEX_ASYNC_WAIT_INTERNAL

			ClusterProcessing_WritingDone();

			bBatchProcessingEnabled = false;
			if (NextStateId == PCGExMT::State_Done) { Done(); }
			if (bIsNextStateAsync) { SetAsyncState(NextStateId); }
			else { SetState(NextStateId); }
		}
	}

	return false;
}

bool FPCGExEdgesProcessorContext::CompileGraphBuilders(const bool bOutputToContext, const PCGExMT::AsyncState NextStateId)
{
	if (IsState(PCGExGraph::State_ReadyToCompile))
	{
		SetAsyncState(PCGExGraph::State_Compiling);
		for (const TSharedPtr<PCGExClusterMT::FClusterProcessorBatchBase>& Batch : Batches) { Batch->CompileGraphBuilder(bOutputToContext); }
		return false;
	}

	if (IsState(PCGExGraph::State_Compiling))
	{
		PCGEX_ASYNC_WAIT_INTERNAL

		ClusterProcessing_GraphCompilationDone();
		SetState(NextStateId);
	}

	return true;
}

bool FPCGExEdgesProcessorContext::HasValidHeuristics() const
{
	TArray<TObjectPtr<const UPCGExParamFactoryBase>> InputFactories;
	const bool bFoundAny = PCGExFactories::GetInputFactories(
		this, PCGExGraph::SourceHeuristicsLabel, InputFactories,
		{PCGExFactories::EType::Heuristics}, false);
	InputFactories.Empty();
	return bFoundAny;
}

void FPCGExEdgesProcessorContext::AdvanceBatch(const PCGExMT::AsyncState NextStateId, const bool bIsNextStateAsync)
{
	CurrentBatchIndex++;
	if (!Batches.IsValidIndex(CurrentBatchIndex))
	{
		CurrentBatch = nullptr;
		bBatchProcessingEnabled = false;
		if (NextStateId == PCGExMT::State_Done) { Done(); }
		if (bIsNextStateAsync) { SetAsyncState(NextStateId); }
		else { SetState(NextStateId); }
	}
	else
	{
		CurrentBatch = Batches[CurrentBatchIndex];
		ScheduleBatch(GetAsyncManager(), CurrentBatch, bScopedIndexLookupBuild);
		SetAsyncState(PCGExClusterMT::MTState_ClusterProcessing);
	}
}

void FPCGExEdgesProcessorContext::OutputPointsAndEdges() const
{
	MainPoints->OutputToContext();
	MainEdges->OutputToContext();
}

int32 FPCGExEdgesProcessorContext::GetClusterProcessorsNum() const
{
	int32 Num = 0;
	for (const TSharedPtr<PCGExClusterMT::FClusterProcessorBatchBase>& Batch : Batches) { Num += Batch->GetNumProcessors(); }
	return Num;
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

	Context->bScopedIndexLookupBuild = Settings->bScopedIndexLookupBuild;

	if (!Settings->bEnabled) { return Context; }

	Context->InputDictionary = MakeUnique<PCGExData::FPointIOTaggedDictionary>(PCGExGraph::TagStr_ClusterPair);

	TArray<TSharedPtr<PCGExData::FPointIO>> TaggedVtx;
	TArray<TSharedPtr<PCGExData::FPointIO>> TaggedEdges;

	Context->MainEdges = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->MainEdges->DefaultOutputLabel = PCGExGraph::OutputEdgesLabel;
	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExGraph::SourceEdgesLabel);
	Context->MainEdges->Initialize(Sources, Settings->GetEdgeOutputInitMode());

	// Gather Vtx inputs
	for (const TSharedPtr<PCGExData::FPointIO>& MainIO : Context->MainPoints->Pairs)
	{
		if (MainIO->Tags->IsTagged(PCGExGraph::TagStr_PCGExVtx))
		{
			if (MainIO->Tags->IsTagged(PCGExGraph::TagStr_PCGExEdges))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Uh oh, a data is marked as both Vtx and Edges -- it will be ignored for safety."));
				continue;
			}

			TaggedVtx.Add(MainIO);
			continue;
		}

		if (MainIO->Tags->IsTagged(PCGExGraph::TagStr_PCGExEdges))
		{
			if (MainIO->Tags->IsTagged(PCGExGraph::TagStr_PCGExVtx))
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
	for (const TSharedPtr<PCGExData::FPointIO>& MainIO : Context->MainEdges->Pairs)
	{
		if (MainIO->Tags->IsTagged(PCGExGraph::TagStr_PCGExEdges))
		{
			if (MainIO->Tags->IsTagged(PCGExGraph::TagStr_PCGExVtx))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Uh oh, a data is marked as both Vtx and Edges. It will be ignored."));
				continue;
			}

			TaggedEdges.Add(MainIO);
			continue;
		}

		if (MainIO->Tags->IsTagged(PCGExGraph::TagStr_PCGExVtx))
		{
			if (MainIO->Tags->IsTagged(PCGExGraph::TagStr_PCGExEdges))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Uh oh, a data is marked as both Vtx and Edges. It will be ignored."));
				continue;
			}

			PCGE_LOG(Warning, GraphAndLog, FTEXT("Uh oh, some Edge data made its way to the vtx input. It will be ignored."));
			continue;
		}

		PCGE_LOG(Warning, GraphAndLog, FTEXT("A data pluggued into Edges is neither tagged Edges or Vtx and will be ignored."));
	}


	for (const TSharedPtr<PCGExData::FPointIO>& Vtx : TaggedVtx)
	{
		if (!PCGExGraph::IsPointDataVtxReady(Vtx->GetIn()->Metadata))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("A Vtx input has no metadata and will be discarded."));
			Vtx->Disable();
			continue;
		}

		if (!Context->InputDictionary->CreateKey(Vtx.ToSharedRef()))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("At least two Vtx inputs share the same PCGEx/Cluster tag. Only one will be processed."));
			Vtx->Disable();
		}
	}

	for (const TSharedPtr<PCGExData::FPointIO>& Edges : TaggedEdges)
	{
		if (!PCGExGraph::IsPointDataEdgeReady(Edges->GetIn()->Metadata))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("An Edges input has no edge metadata and will be discarded."));
			Edges->Disable();
			continue;
		}

		if (!Context->InputDictionary->TryAddEntry(Edges.ToSharedRef()))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input edges have no associated vtx."));
		}
	}

	return Context;
}

#undef LOCTEXT_NAMESPACE
