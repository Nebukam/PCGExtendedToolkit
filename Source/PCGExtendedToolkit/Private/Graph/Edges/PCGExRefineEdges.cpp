﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExRefineEdges.h"


#include "PCGParamData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Graph/PCGExGraph.h"
#include "Graph/Edges/Refining/PCGExEdgeRefinePrimMST.h"
#include "Graph/Filters/PCGExClusterFilter.h"

#define LOCTEXT_NAMESPACE "PCGExRefineEdges"
#define PCGEX_NAMESPACE RefineEdges

TArray<FPCGPinProperties> UPCGExRefineEdgesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (Refinement && Refinement->WantsHeuristics()) { PCGEX_PIN_FACTORIES(PCGExGraph::SourceHeuristicsLabel, "Heuristics may be required by some refinements.", Required, FPCGExDataTypeInfoHeuristics::AsId()) }
	if (Refinement && Refinement->SupportFilters())
	{
		PCGEX_PIN_FILTERS(PCGExGraph::SourceEdgeFiltersLabel, "Refinements filters.", Normal)
	}

	if (Sanitization == EPCGExRefineSanitization::Filters)
	{
		PCGEX_PIN_FILTERS(PCGExRefineEdges::SourceSanitizeEdgeFilters, "Filters that define which edges are to be kept. During the sanitization step, edges that pass the filters are restored if they were previously removed.", Required)
	}

	PCGEX_PIN_OPERATION_OVERRIDES(PCGExRefineEdges::SourceOverridesRefinement)

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExRefineEdgesSettings::OutputPinProperties() const
{
	if (!bOutputEdgesOnly) { return Super::OutputPinProperties(); }

	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputKeptEdgesLabel, "Kept edges but as simple points.", Required)
	PCGEX_PIN_POINTS(PCGExGraph::OutputRemovedEdgesLabel, "Removed edges but as simple points.", Required)
	return PinProperties;
}

PCGExData::EIOInit UPCGExRefineEdgesSettings::GetMainOutputInitMode() const { return bOutputEdgesOnly ? PCGExData::EIOInit::NoInit : PCGExData::EIOInit::New; }
PCGExData::EIOInit UPCGExRefineEdgesSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::NoInit; }

PCGEX_INITIALIZE_ELEMENT(RefineEdges)
PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(RefineEdges)

bool FPCGExRefineEdgesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(RefineEdges)

	if (!Settings->Refinement)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No refinement selected."));
		return false;
	}

	PCGEX_OPERATION_BIND(Refinement, UPCGExEdgeRefineInstancedFactory, PCGExRefineEdges::SourceOverridesRefinement)
	PCGEX_FWD(GraphBuilderDetails)

	if (Context->Refinement->WantsHeuristics() && !Context->bHasValidHeuristics)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("The selected refinement requires heuristics to be connected, but none can be found."));
		return false;
	}

	if (Context->Refinement->SupportFilters())
	{
		//GetInputFactories(Context, PCGExRefineEdges::SourceVtxFilters, Context->VtxFilterFactories, PCGExFactories::ClusterNodeFilters, false);
		GetInputFactories(
			Context, PCGExGraph::SourceEdgeFiltersLabel, Context->EdgeFilterFactories,
			PCGExFactories::ClusterEdgeFilters, false);
	}

	if (Settings->Sanitization == EPCGExRefineSanitization::Filters)
	{
		if (!GetInputFactories(
			Context, PCGExRefineEdges::SourceSanitizeEdgeFilters, Context->SanitizationFilterFactories,
			PCGExFactories::ClusterEdgeFilters))
		{
			return false;
		}
	}

	if (Settings->bOutputEdgesOnly)
	{
		// TODO : Revisit this

		Context->KeptEdges = MakeShared<PCGExData::FPointIOCollection>(Context);
		Context->KeptEdges->OutputPin = PCGExGraph::OutputKeptEdgesLabel;

		Context->RemovedEdges = MakeShared<PCGExData::FPointIOCollection>(Context);
		Context->RemovedEdges->OutputPin = PCGExGraph::OutputRemovedEdgesLabel;

		int32 NumEdgesInputs = Context->MainEdges->Num();
		Context->KeptEdges->Pairs.Reserve(NumEdgesInputs);
		Context->RemovedEdges->Pairs.Reserve(NumEdgesInputs);

		for (const TSharedPtr<PCGExData::FPointIO>& EdgeIO : Context->MainEdges->Pairs)
		{
			Context->KeptEdges->Emplace_GetRef(EdgeIO, PCGExData::EIOInit::New)->bAllowEmptyOutput = Settings->bAllowZeroPointOutputs;
			Context->RemovedEdges->Emplace_GetRef(EdgeIO, PCGExData::EIOInit::New)->bAllowEmptyOutput = Settings->bAllowZeroPointOutputs;
		}
	}

	return true;
}

bool FPCGExRefineEdgesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExRefineEdgesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(RefineEdges)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
			{
				NewBatch->GraphBuilderDetails = Context->GraphBuilderDetails;
				if (Context->Refinement->WantsHeuristics()) { NewBatch->SetWantsHeuristics(true); }
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(Settings->bOutputEdgesOnly ? PCGExCommon::State_Done : PCGExGraph::State_ReadyToCompile)

	if (!Settings->bOutputEdgesOnly && !Context->CompileGraphBuilders(true, PCGExCommon::State_Done)) { return false; }

	if (!Settings->bOutputEdgesOnly)
	{
		Context->MainPoints->StageOutputs();
	}
	else
	{
		Context->KeptEdges->StageOutputs();
		Context->RemovedEdges->StageOutputs();
	}

	return Context->TryComplete();
}

namespace PCGExRefineEdges
{
	TSharedPtr<PCGExCluster::FCluster> FProcessor::HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef)
	{
		// Create a light working copy with edges only, will be deleted.
		return MakeShared<PCGExCluster::FCluster>(
			InClusterRef, VtxDataFacade->Source, EdgeDataFacade->Source, NodeIndexLookup,
			false, true, false);
	}

	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExRefineEdges::Process);

		EdgeFilterFactories = &Context->EdgeFilterFactories; // So filters can be initialized

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		Sanitization = Settings->Sanitization;

		Refinement = Context->Refinement->CreateOperation();
		if (!Refinement) { return false; }

		Refinement->PrimaryDataFacade = VtxDataFacade;
		Refinement->SecondaryDataFacade = EdgeDataFacade;

		Refinement->PrepareForCluster(Cluster, HeuristicsHandler);

		Refinement->VtxFilterCache = VtxFilterCache.Get();
		Refinement->EdgeFilterCache = &EdgeFilterCache;

		const int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize();

		if (Settings->Sanitization == EPCGExRefineSanitization::Filters)
		{
			if (!Context->SanitizationFilterFactories.IsEmpty())
			{
				SanitizationFilterManager = MakeShared<PCGExClusterFilter::FManager>(Cluster.ToSharedRef(), VtxDataFacade, EdgeDataFacade);
				SanitizationFilterManager->bUseEdgeAsPrimary = true;
				SanitizationFilterManager->SetSupportedTypes(&PCGExFactories::ClusterEdgeFilters);
				if (!SanitizationFilterManager->Init(ExecutionContext, Context->SanitizationFilterFactories)) { return false; }
			}
		}

		// Need to go through PrepareSingleLoopScopeForEdges anyway

		if (Context->Refinement->WantsIndividualEdgeProcessing())
		{
			StartParallelLoopForEdges();
		}
		else
		{
			PCGEX_ASYNC_GROUP_CHKD(AsyncManager, EdgeScopeLoop)

			EdgeScopeLoop->OnCompleteCallback =
				[PCGEX_ASYNC_THIS_CAPTURE]()
				{
					PCGEX_ASYNC_THIS
					if (This->Context->Refinement->WantsIndividualNodeProcessing()) { This->StartParallelLoopForNodes(); }
					else { This->Refinement->Process(); }
				};

			EdgeScopeLoop->OnSubLoopStartCallback =
				[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
				{
					PCGEX_ASYNC_THIS
					This->PrepareSingleLoopScopeForEdges(Scope);
				};

			EdgeScopeLoop->StartSubLoops(EdgeDataFacade->GetNum(), PLI);
		}

		return true;
	}

	void FProcessor::ProcessNodes(const PCGExMT::FScope& Scope)
	{
		TArray<PCGExCluster::FNode>& Nodes = *Cluster->Nodes;

		PCGEX_SCOPE_LOOP(Index)
		{
			Refinement->ProcessNode(Nodes[Index]);
		}
	}

	void FProcessor::PrepareSingleLoopScopeForEdges(const PCGExMT::FScope& Scope)
	{
		EdgeDataFacade->Fetch(Scope);
		FilterEdgeScope(Scope);

		TArray<PCGExGraph::FEdge>& Edges = *Cluster->Edges;

		const bool bDefaultValidity = Context->Refinement->GetDefaultEdgeValidity();
		PCGEX_SCOPE_LOOP(i) { Edges[i].bValid = bDefaultValidity; }
	}

	void FProcessor::ProcessEdges(const PCGExMT::FScope& Scope)
	{
		PrepareSingleLoopScopeForEdges(Scope);

		PCGEX_SCOPE_LOOP(Index)
		{
			PCGExGraph::FEdge& Edge = *Cluster->GetEdge(Index);
			Refinement->ProcessEdge(Edge);
		}
	}

	void FProcessor::OnEdgesProcessingComplete()
	{
		if (!Settings->bRestoreEdgesThatConnectToValidNodes) { return; }

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, InvalidateNodes)

		InvalidateNodes->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				const PCGExCluster::FCluster* LocalCluster = This->Cluster.Get();
				PCGEX_SCOPE_LOOP(i)
				{
					if (PCGExCluster::FNode* Node = LocalCluster->GetNode(i); !Node->HasAnyValidEdges(LocalCluster)) { Node->bValid = false; }
				}
			};

		InvalidateNodes->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				PCGEX_ASYNC_GROUP_CHKD_VOID(This->AsyncManager, RestoreEdges)
				RestoreEdges->OnSubLoopStartCallback =
					[AsyncThis](const PCGExMT::FScope& Scope)
					{
						PCGEX_ASYNC_NESTED_THIS
						const PCGExCluster::FCluster* LocalCluster = NestedThis->Cluster.Get();

						PCGEX_SCOPE_LOOP(i)
						{
							PCGExGraph::FEdge* Edge = LocalCluster->GetEdge(i);
							if (Edge->bValid) { continue; }
							if (LocalCluster->GetEdgeStart(i)->bValid && LocalCluster->GetEdgeEnd(i)->bValid) { Edge->bValid = true; }
						}
					};

				RestoreEdges->StartSubLoops(This->Cluster->Edges->Num(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
			};


		InvalidateNodes->StartSubLoops(Cluster->Nodes->Num(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}

	void FProcessor::Sanitize()
	{
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, SanitizeTaskGroup)

		Cluster->GetBoundedEdges(true); //Oof

		SanitizeTaskGroup->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->InsertEdges();
			};

		if (Settings->Sanitization == EPCGExRefineSanitization::Filters)
		{
			const int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize();
			SanitizeTaskGroup->OnSubLoopStartCallback =
				[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
				{
					PCGEX_ASYNC_THIS

					const TSharedPtr<PCGExCluster::FCluster> LocalCluster = This->Cluster;
					const TSharedPtr<PCGExClusterFilter::FManager> SanitizationFilters = This->SanitizationFilterManager;

					PCGEX_SCOPE_LOOP(i)
					{
						PCGExGraph::FEdge& Edge = *LocalCluster->GetEdge(i);
						if (SanitizationFilters->Test(Edge)) { Edge.bValid = true; }
					}
				};
			SanitizeTaskGroup->StartSubLoops(EdgeDataFacade->GetNum(), PLI);
		}
		else
		{
			PCGEX_SHARED_THIS_DECL
			SanitizeTaskGroup->StartRanges<FSanitizeRangeTask>(
				NumNodes, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize(),
				false, ThisPtr);
		}
	}

	void FProcessor::InsertEdges() const
	{
		if (GraphBuilder)
		{
			TArray<PCGExGraph::FEdge> ValidEdges;
			Cluster->GetValidEdges(ValidEdges);

			if (ValidEdges.IsEmpty()) { return; }

			GraphBuilder->Graph->InsertEdges(ValidEdges);
			return;
		}

		const UPCGBasePointData* OriginalEdges = EdgeDataFacade->GetIn();

		TBitArray<> Mask;
		Mask.Init(false, OriginalEdges->GetNumPoints());

		const TArray<PCGExGraph::FEdge>& Edges = *Cluster->Edges;
		for (int i = 0; i < Mask.Num(); ++i) { Mask[i] = Edges[i].bValid ? true : false; }

		(void)Context->KeptEdges->Pairs[EdgeDataFacade->Source->IOIndex]->InheritPoints(Mask, false);
		(void)Context->RemovedEdges->Pairs[EdgeDataFacade->Source->IOIndex]->InheritPoints(Mask, true);
	}

	void FProcessor::CompleteWork()
	{
		if (Settings->Sanitization != EPCGExRefineSanitization::None)
		{
			Sanitize();
			return;
		}

		InsertEdges();
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExRefineEdgesContext, UPCGExRefineEdgesSettings>::Cleanup();
		Refinement.Reset();
		SanitizationFilterManager.Reset();
	}

	void FBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(RefineEdges)

		Context->Refinement->RegisterBuffersDependencies(ExecutionContext, FacadePreloader);

		//PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->VtxFilterFactories, FacadePreloader);
		PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->EdgeFilterFactories, FacadePreloader);
		PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->SanitizationFilterFactories, FacadePreloader);
	}

	void FBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(RefineEdges)

		Context->Refinement->PrepareVtxFacade(VtxDataFacade);
		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}

	void FSanitizeRangeTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		auto RestoreEdge = [&](const int32 EdgeIndex)
		{
			if (EdgeIndex == -1) { return; }
			FPlatformAtomics::InterlockedExchange(&Processor->Cluster->GetEdge(EdgeIndex)->bValid, 1);
			FPlatformAtomics::InterlockedExchange(&Processor->Cluster->GetEdgeStart(EdgeIndex)->bValid, 1);
			FPlatformAtomics::InterlockedExchange(&Processor->Cluster->GetEdgeEnd(EdgeIndex)->bValid, 1);
		};

		if (Processor->Sanitization == EPCGExRefineSanitization::Longest)
		{
			PCGEX_SCOPE_LOOP(i)
			{
				const PCGExCluster::FNode* Node = (Processor->Cluster->GetNode(i));

				int32 BestIndex = -1;
				double LongestDist = 0;

				for (const PCGExGraph::FLink Lk : Node->Links)
				{
					const double Dist = Processor->Cluster->GetDistSquared(Node->Index, Lk.Node);
					if (Dist > LongestDist)
					{
						LongestDist = Dist;
						BestIndex = Lk.Edge;
					}
				}

				RestoreEdge(BestIndex);
			}
		}
		else if (Processor->Sanitization == EPCGExRefineSanitization::Shortest)
		{
			PCGEX_SCOPE_LOOP(i)
			{
				const PCGExCluster::FNode* Node = Processor->Cluster->GetNode(i);

				int32 BestIndex = -1;
				double ShortestDist = MAX_dbl;

				for (const PCGExGraph::FLink Lk : Node->Links)
				{
					const double Dist = Processor->Cluster->GetDistSquared(Node->Index, Lk.Node);
					if (Dist < ShortestDist)
					{
						ShortestDist = Dist;
						BestIndex = Lk.Edge;
					}
				}

				RestoreEdge(BestIndex);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
