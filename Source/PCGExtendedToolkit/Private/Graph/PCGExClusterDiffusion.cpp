// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExClusterDiffusion.h"

#include "Dataflow/DataflowGraph.h"

#define LOCTEXT_NAMESPACE "PCGExClusterDiffusion"
#define PCGEX_NAMESPACE ClusterDiffusion

PCGExData::EIOInit UPCGExClusterDiffusionSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }
PCGExData::EIOInit UPCGExClusterDiffusionSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Forward; }

TArray<FPCGPinProperties> UPCGExClusterDiffusionSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	PCGEX_PIN_FACTORIES(PCGExDataBlending::SourceBlendingLabel, "Blending configurations.", Required, {})
	PCGEX_PIN_FACTORIES(PCGExGraph::SourceHeuristicsLabel, "Heuristics.", Required, {})

	if (Seeds == EPCGExDiffusionSeeds::Filters)
	{
		// Add filter input
		PCGEX_PIN_FACTORIES(PCGExPointFilter::SourceFiltersLabel, "Filters used to pick and choose which vtx will be used as seeds", Required, {})

		if (Ordering == EPCGExDiffusionOrder::Sorting)
		{
			PCGEX_PIN_FACTORIES(PCGExSorting::SourceSortingRules, "Plug sorting rules here. Order is defined by each rule' priority value, in ascending order.", Required, {})
		}
	}
	else
	{
		PCGEX_PIN_POINT(PCGExGraph::SourceSeedsLabel, "Seed points.", Required, {})
	}

	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(ClusterDiffusion)

bool FPCGExClusterDiffusionElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ClusterDiffusion)
	PCGEX_FOREACH_FIELD_CLUSTER_DIFF(PCGEX_OUTPUT_VALIDATE_NAME)

	if (!PCGExFactories::GetInputFactories<UPCGExAttributeBlendFactory>(
		Context, PCGExDataBlending::SourceBlendingLabel, Context->BlendingFactories,
		{PCGExFactories::EType::Blending}, true))
	{
		return false;
	}

	if (Settings->Seeds == EPCGExDiffusionSeeds::Points)
	{
		Context->SeedsDataFacade = PCGExData::TryGetSingleFacade(Context, PCGExGraph::SourceSeedsLabel, true);
		if (!Context->SeedsDataFacade) { return false; }

		Context->SeedForwardHandler = Settings->SeedForwarding.GetHandler(Context->SeedsDataFacade);
	}

	return true;
}

bool FPCGExClusterDiffusionElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExClusterDiffusionElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ClusterDiffusion)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExClusterDiffusion::FBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterDiffusion::FBatch>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExClusterDiffusion
{
	FDiffusion::FDiffusion(const TSharedPtr<PCGExCluster::FCluster>& InCluster, const PCGExCluster::FNode* InSeedNode)
		: Cluster(InCluster), SeedNode(InSeedNode)
	{
	}

	void FDiffusion::Init()
	{
		Visited.Add(SeedNode->Index);
		FCandidate SeedCandidate = FCandidate();
		SeedCandidate.Node = SeedNode;

		Probe(SeedCandidate);
	}

	void FDiffusion::Probe(const FCandidate& From)
	{
		// Gather all neighbors and compute heuristics, add to candidate for the first time only
		bool bIsAlreadyInSet = false;

		const PCGExCluster::FNode& Seed = *From.Node;
		const PCGExCluster::FNode& RoamingGoal = *Processor->HeuristicsHandler->GetRoamingGoal();

		for (const PCGExGraph::FLink& Lk : Seed.Links)
		{
			PCGExCluster::FNode* OtherNode = Cluster->GetNode(Lk);
			Visited.Add(OtherNode->Index, &bIsAlreadyInSet);

			if (bIsAlreadyInSet) { continue; }

			// TODO : Make sure candidate is within set limits, otherwise continue

			FCandidate& Candidate = Candidates.Emplace_GetRef();
			Candidate.Node = OtherNode;
			Candidate.Score = Processor->HeuristicsHandler->GetEdgeScore(
				Seed, *OtherNode,
				*Cluster->GetEdge(Lk), *SeedNode, RoamingGoal,
				nullptr, TravelStack);
			Candidate.Depth = From.Depth + 1;
			Candidate.Distance = From.Distance + 10; // TODO : Compute distance
		}
	}

	void FDiffusion::Capture()
	{
		bool bSearch = true;
		while (bSearch)
		{
			if(Candidates.IsEmpty())
			{
				bStopped = true;
				break;
			}
			FCandidate Candidate = Candidates.Pop();
			
			if (*(Processor->InfluencesCount->GetData() + Candidate.Node->PointIndex) >= 1) { continue; }

			bSearch = false;
		}
		// TODO : Sanity check candidates
		// Check available candidates
		// If no candidate available, find next layer of candidates
	}

	void FDiffusion::Complete(FPCGExClusterDiffusionContext* Context, const TSharedPtr<PCGExData::FFacade>& InVtxFacade)
	{
		if (SeedIndex != -1)
		{
			TArray<int32> Indices; // TODO : Nodes to pt index
			Context->SeedForwardHandler->Forward(SeedIndex, InVtxFacade, Indices);
		}
	}

	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExClusterDiffusion::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }
		// First, build a list of diffusions
		// Either from seeds (find closest) or filters


		return true;
	}

	void FProcessor::Grow()
	{
		if (Settings->Processing == EPCGExDiffusionProcessing::Parallel)
		{
			StartParallelLoopForRange(OngoingDiffusions.Num());
			return;
		}
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope)
	{
		// TODO : Local growth : find & sort all candidates
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		// TODO : 
	}

	FBatch::FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
		: TBatchWithHeuristics(InContext, InVtx, InEdges)
	{
	}

	FBatch::~FBatch()
	{
	}

	void FBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ClusterDiffusion)

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = VtxDataFacade;
			PCGEX_FOREACH_FIELD_CLUSTER_DIFF(PCGEX_OUTPUT_INIT)
		}

		for (const TObjectPtr<const UPCGExAttributeBlendFactory>& Factory : Context->BlendingFactories)
		{
			Factory->RegisterBuffersDependencies(Context, FacadePreloader);
		}
	}

	void FBatch::Process()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ClusterDiffusion)

		Operations = MakeShared<TArray<UPCGExAttributeBlendOperation*>>();
		Operations->Reserve(Context->BlendingFactories.Num());

		for (const TObjectPtr<const UPCGExAttributeBlendFactory>& Factory : Context->BlendingFactories)
		{
			UPCGExAttributeBlendOperation* Op = Factory->CreateOperation(Context);
			if (!Op)
			{
				bIsBatchValid = false;
				PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("An operation could not be created."));
				return; // FAIL
			}

			Op->OpIdx = Operations->Add(Op);
			Op->SiblingOperations = Operations;

			if (!Op->PrepareForData(Context, VtxDataFacade))
			{
				bIsBatchValid = false;
				return; // FAIL
			}
		}

		TBatchWithHeuristics<FProcessor>::Process();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor)
	{
		if (!TBatch<FProcessor>::PrepareSingle(ClusterProcessor)) { return false; }

		ClusterProcessor->Operations = Operations;
		ClusterProcessor->InfluencesCount = InfluencesCount;

#define PCGEX_OUTPUT_FWD_TO(_NAME, _TYPE, _DEFAULT_VALUE) if(_NAME##Writer){ ClusterProcessor->_NAME##Writer = _NAME##Writer; }
		PCGEX_FOREACH_FIELD_CLUSTER_DIFF(PCGEX_OUTPUT_FWD_TO)
#undef PCGEX_OUTPUT_FWD_TO

		return true;
	}

	void FBatch::Write()
	{
		TBatch<FProcessor>::Write();
		VtxDataFacade->Write(AsyncManager);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
