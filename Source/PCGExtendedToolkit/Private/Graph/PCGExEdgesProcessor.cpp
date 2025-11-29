// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExEdgesProcessor.h"


#include "Data/PCGExData.h"
#include "Data/PCGExPointFilter.h"
#include "Data/PCGExPointIO.h"
#include "Graph/PCGExClusterMT.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicsFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface


PCGExData::EIOInit UPCGExEdgesProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Forward; }
PCGExData::EIOInit UPCGExEdgesProcessorSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Forward; }

bool UPCGExEdgesProcessorSettings::GetMainAcceptMultipleData() const { return true; }

bool UPCGExEdgesProcessorSettings::WantsScopedIndexLookupBuild() const
{
	PCGEX_GET_OPTION_STATE(ScopedIndexLookupBuild, bDefaultScopedIndexLookupBuild)
}

FPCGExEdgesProcessorContext::~FPCGExEdgesProcessorContext()
{
	PCGEX_TERMINATE_ASYNC

	for (TSharedPtr<PCGExClusterMT::IBatch>& Batch : Batches)
	{
		Batch->Cleanup();
		Batch.Reset();
	}

	Batches.Empty();
}

TArray<FPCGPinProperties> UPCGExEdgesProcessorSettings::InputPinProperties() const
{
	//TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	TArray<FPCGPinProperties> PinProperties;

	if (!IsInputless())
	{
		if (GetMainAcceptMultipleData()) { PCGEX_PIN_POINTS(GetMainInputPin(), "The point data to be processed.", Required) }
		else { PCGEX_PIN_POINT(GetMainInputPin(), "The point data to be processed.", Required) }
	}

	PCGEX_PIN_POINTS(PCGExGraph::SourceEdgesLabel, "Edges associated with the main input points", Required)

	if (SupportsPointFilters())
	{
		if (RequiresPointFilters()) { PCGEX_PIN_FILTERS(GetPointFilterPin(), GetPointFilterTooltip(), Required) }
		else { PCGEX_PIN_FILTERS(GetPointFilterPin(), GetPointFilterTooltip(), Normal) }
	}

	if (SupportsEdgeSorting())
	{
		if (RequiresEdgeSorting()) { PCGEX_PIN_FACTORIES(PCGExGraph::SourceEdgeSortingRules, "Plug sorting rules here. Order is defined by each rule' priority value, in ascending order.", Required, FPCGExDataTypeInfoSortRule::AsId()) }
		else { PCGEX_PIN_FACTORIES(PCGExGraph::SourceEdgeSortingRules, "Plug sorting rules here. Order is defined by each rule' priority value, in ascending order.", Normal, FPCGExDataTypeInfoSortRule::AsId()) }
	}
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExEdgesProcessorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Edges associated with the main output points", Required)
	return PinProperties;
}

bool UPCGExEdgesProcessorSettings::SupportsEdgeSorting() const { return false; }

bool UPCGExEdgesProcessorSettings::RequiresEdgeSorting() const { return true; }

#pragma endregion

const TArray<FPCGExSortRuleConfig>* FPCGExEdgesProcessorContext::GetEdgeSortingRules() const
{
	if (EdgeSortingRules.IsEmpty()) { return nullptr; }
	return &EdgeSortingRules;
}

bool FPCGExEdgesProcessorContext::AdvancePointsIO(const bool bCleanupKeys)
{
	CurrentCluster.Reset();

	CurrentEdgesIndex = -1;

	if (!FPCGExPointsProcessorContext::AdvancePointsIO(bCleanupKeys)) { return false; }

	TaggedEdges = ClusterDataLibrary->GetAssociatedEdges(CurrentIO);

	if (TaggedEdges && !TaggedEdges->Entries.IsEmpty())
	{
		PCGExCommon::DataIDType OutId;
		PCGExGraph::SetClusterVtx(CurrentIO, OutId); // Update key
		PCGExGraph::MarkClusterEdges(TaggedEdges->Entries, OutId);
	}
	else { TaggedEdges = nullptr; }

	if (!TaggedEdges && !bQuietMissingClusterPairElement)
	{
		PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Some input vtx have no associated edges."));
	}

	return true;
}

void FPCGExEdgesProcessorContext::OutputBatches() const
{
	for (const TSharedPtr<PCGExClusterMT::IBatch>& Batch : Batches) { Batch->Output(); }
}

TSharedPtr<PCGExClusterMT::IBatch> FPCGExEdgesProcessorContext::CreateEdgeBatchInstance(const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges) const
{
	return nullptr;
}

bool FPCGExEdgesProcessorContext::ProcessClusters(const PCGExCommon::ContextState NextStateId, const bool bIsNextStateAsync)
{
	//FWriteScopeLock WriteScopeLock(ClusterProcessingLock); // Just in case

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

		PCGEX_ON_ASYNC_STATE_READY_INTERNAL(PCGExClusterMT::MTState_ClusterProcessing)
		{
			if (!CurrentBatch->bSkipCompletion)
			{
				SetAsyncState(PCGExClusterMT::MTState_ClusterCompletingWork);
				CurrentBatch->CompleteWork();
			}
			else
			{
				SetState(PCGExClusterMT::MTState_ClusterCompletingWork);
			}
		}

		PCGEX_ON_ASYNC_STATE_READY_INTERNAL(PCGExClusterMT::MTState_ClusterCompletingWork)
		{
			AdvanceBatch(NextStateId, bIsNextStateAsync);
		}
	}
	else
	{
		PCGEX_ON_ASYNC_STATE_READY_INTERNAL(PCGExClusterMT::MTState_ClusterProcessing)
		{
			ClusterProcessing_InitialProcessingDone();

			if (!bSkipClusterBatchCompletionStep)
			{
				SetAsyncState(PCGExClusterMT::MTState_ClusterCompletingWork);
				CompleteBatches(Batches);
			}
			else
			{
				SetState(PCGExClusterMT::MTState_ClusterCompletingWork);
			}
		}

		PCGEX_ON_ASYNC_STATE_READY_INTERNAL(PCGExClusterMT::MTState_ClusterCompletingWork)
		{
			if (!bSkipClusterBatchCompletionStep) { ClusterProcessing_WorkComplete(); }

			if (bDoClusterBatchWritingStep)
			{
				SetAsyncState(PCGExClusterMT::MTState_ClusterWriting);
				WriteBatches(Batches);
				return false;
			}

			bBatchProcessingEnabled = false;
			if (NextStateId == PCGExCommon::State_Done) { Done(); }
			if (bIsNextStateAsync) { SetAsyncState(NextStateId); }
			else { SetState(NextStateId); }
		}

		PCGEX_ON_ASYNC_STATE_READY_INTERNAL(PCGExClusterMT::MTState_ClusterWriting)
		{
			ClusterProcessing_WritingDone();

			bBatchProcessingEnabled = false;
			if (NextStateId == PCGExCommon::State_Done) { Done(); }
			if (bIsNextStateAsync) { SetAsyncState(NextStateId); }
			else { SetState(NextStateId); }
		}
	}

	return false;
}

bool FPCGExEdgesProcessorContext::CompileGraphBuilders(const bool bOutputToContext, const PCGExCommon::ContextState NextStateId)
{
	PCGEX_ON_STATE_INTERNAL(PCGExGraph::State_ReadyToCompile)
	{
		SetAsyncState(PCGExGraph::State_Compiling);
		for (const TSharedPtr<PCGExClusterMT::IBatch>& Batch : Batches) { Batch->CompileGraphBuilder(bOutputToContext); }
		return false;
	}

	PCGEX_ON_ASYNC_STATE_READY_INTERNAL(PCGExGraph::State_Compiling)
	{
		ClusterProcessing_GraphCompilationDone();
		SetState(NextStateId);
	}

	return true;
}

bool FPCGExEdgesProcessorContext::StartProcessingClusters(FBatchProcessingValidateEntries&& ValidateEntries, FBatchProcessingInitEdgeBatch&& InitBatch, const bool bInlined)
{
	ResumeExecution();

	Batches.Empty();

	bClusterBatchInlined = bInlined;
	CurrentBatchIndex = -1;

	bBatchProcessingEnabled = false;
	bClusterWantsHeuristics = true;
	bSkipClusterBatchCompletionStep = false;
	bDoClusterBatchWritingStep = false;

	Batches.Reserve(MainPoints->Pairs.Num());

	EdgesDataFacades.Reserve(MainEdges->Pairs.Num());
	for (const TSharedPtr<PCGExData::FPointIO>& EdgeIO : MainEdges->Pairs)
	{
		TSharedPtr<PCGExData::FFacade> EdgeFacade = MakeShared<PCGExData::FFacade>(EdgeIO.ToSharedRef());
		EdgesDataFacades.Add(EdgeFacade.ToSharedRef());
	}

	while (AdvancePointsIO(false))
	{
		if (!TaggedEdges)
		{
			PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Some input points have no bound edges."));
			continue;
		}

		if (!ValidateEntries(TaggedEdges)) { continue; }

		const TSharedPtr<PCGExClusterMT::IBatch> NewBatch = CreateEdgeBatchInstance(CurrentIO.ToSharedRef(), TaggedEdges->Entries);
		InitBatch(NewBatch);

		if (NewBatch->bRequiresWriteStep) { bDoClusterBatchWritingStep = true; }
		if (NewBatch->bSkipCompletion) { bSkipClusterBatchCompletionStep = true; }
		if (NewBatch->RequiresGraphBuilder()) { NewBatch->GraphBuilderDetails = GraphBuilderDetails; }

		if (NewBatch->WantsHeuristics())
		{
			bClusterWantsHeuristics = true;
			if (!bHasValidHeuristics)
			{
				PCGEX_LOG_MISSING_INPUT(this, FTEXT("Missing Heuristics."))
				return false;
			}
			NewBatch->HeuristicsFactories = &HeuristicsFactories;
		}

		NewBatch->EdgesDataFacades = &EdgesDataFacades;

		Batches.Add(NewBatch);
		if (!bClusterBatchInlined) { PCGExClusterMT::ScheduleBatch(GetAsyncManager(), NewBatch, bScopedIndexLookupBuild); }
	}

	if (Batches.IsEmpty()) { return false; }

	bBatchProcessingEnabled = true;
	if (!bClusterBatchInlined) { SetAsyncState(PCGExClusterMT::MTState_ClusterProcessing); }
	return true;
}

void FPCGExEdgesProcessorContext::ClusterProcessing_InitialProcessingDone()
{
}

void FPCGExEdgesProcessorContext::ClusterProcessing_WorkComplete()
{
}

void FPCGExEdgesProcessorContext::ClusterProcessing_WritingDone()
{
}

void FPCGExEdgesProcessorContext::ClusterProcessing_GraphCompilationDone()
{
}

void FPCGExEdgesProcessorContext::AdvanceBatch(const PCGExCommon::ContextState NextStateId, const bool bIsNextStateAsync)
{
	CurrentBatchIndex++;
	if (!Batches.IsValidIndex(CurrentBatchIndex))
	{
		CurrentBatch = nullptr;
		bBatchProcessingEnabled = false;
		if (NextStateId == PCGExCommon::State_Done) { Done(); }
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
	MainPoints->StageOutputs();
	MainEdges->StageOutputs();
}

int32 FPCGExEdgesProcessorContext::GetClusterProcessorsNum() const
{
	int32 Num = 0;
	for (const TSharedPtr<PCGExClusterMT::IBatch>& Batch : Batches) { Num += Batch->GetNumProcessors(); }
	return Num;
}

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

	Context->bQuietMissingClusterPairElement = Settings->bQuietMissingClusterPairElement;

	Context->bHasValidHeuristics = PCGExFactories::GetInputFactories(
		Context, PCGExGraph::SourceHeuristicsLabel, Context->HeuristicsFactories,
		{PCGExFactories::EType::Heuristics}, false);

	Context->ClusterDataLibrary = MakeShared<PCGExClusterUtils::FClusterDataLibrary>(true);

	TArray<TSharedPtr<PCGExData::FPointIO>> TaggedVtx;
	TArray<TSharedPtr<PCGExData::FPointIO>> TaggedEdges;

	Context->MainEdges = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->MainEdges->OutputPin = PCGExGraph::OutputEdgesLabel;
	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExGraph::SourceEdgesLabel);
	Context->MainEdges->Initialize(Sources, Settings->GetEdgeOutputInitMode());

	if (!Context->ClusterDataLibrary->Build(Context->MainPoints, Context->MainEdges))
	{
		Context->ClusterDataLibrary->PrintLogs(Context);
		PCGEX_LOG_MISSING_INPUT(Context, FTEXT("Could not find any valid vtx/edge pairs."))
		return false;
	}

	if (Settings->SupportsEdgeSorting())
	{
		Context->EdgeSortingRules = PCGExSorting::GetSortingRules(Context, PCGExGraph::SourceEdgeSortingRules);
		if (Settings->RequiresEdgeSorting() && Context->EdgeSortingRules.IsEmpty())
		{
			PCGEX_LOG_MISSING_INPUT(Context, FTEXT("Missing valid sorting rules."))
			return false;
		}
	}

	return true;
}

void FPCGExEdgesProcessorElement::OnContextInitialized(FPCGExContext* InContext) const
{
	FPCGExPointsProcessorElement::OnContextInitialized(InContext);
	PCGEX_CONTEXT_AND_SETTINGS(EdgesProcessor)
	Context->bScopedIndexLookupBuild = Settings->WantsScopedIndexLookupBuild();
}

#undef LOCTEXT_NAMESPACE
