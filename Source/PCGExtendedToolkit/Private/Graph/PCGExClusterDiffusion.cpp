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
		PCGEX_PIN_FACTORIES(PCGExPointFilter::SourceVtxFiltersLabel, "Filters used to pick and choose which vtx will be used as seeds. Supports Regular & Node filters.", Required, {})

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
		PCGExFactories::ClusterNodeFilters, true))
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
				if (Settings->Seeds == EPCGExDiffusionSeeds::Filters) { NewBatch->VtxFilterFactories = &Context->FilterFactories; }
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
		FCandidate& SeedCandidate = Captured.Emplace_GetRef();
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

	void FDiffusion::Grow()
	{
		bool bSearch = true;
		//int32 MaxInfluences = Processor->Settings->MaxInfluences

		while (bSearch)
		{
			if (Candidates.IsEmpty())
			{
				bStopped = true;
				break;
			}

#if PCGEX_ENGINE_VERSION <= 503
			FCandidate Candidate = Candidates.Pop(false);	
#else
			FCandidate Candidate = Candidates.Pop(EAllowShrinking::No);
#endif

			int32& Influences = *(Processor->InfluencesCount->GetData() + Candidate.Node->PointIndex);
			if (Influences >= 1) { continue; } // Validate candidate is still valid

			Influences++;
			Captured.Add(Candidate);
			Probe(Candidate);

			bSearch = false;
		}
		// TODO : Sanity check candidates
		// Check available candidates
		// If no candidate available, find next layer of candidates
	}

	void FDiffusion::SortCandidates()
	{
		// TODO : Sort active candidates
	}

	void FDiffusion::Diffuse()
	{
		// TODO : Blend
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

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, DiffusionInitialization)
		DiffusionInitialization->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->StartGrowth();
			};

		DiffusionInitialization->OnPrepareSubLoopsCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const TArray<PCGExMT::FScope>& Loops)
			{
				PCGEX_ASYNC_THIS
				This->InitialDiffusions = MakeShared<PCGExMT::TScopedArray<TSharedPtr<FDiffusion>>>(Loops);
			};

		if (Settings->Seeds == EPCGExDiffusionSeeds::Filters)
		{
			DiffusionInitialization->OnSubLoopStartCallback =
				[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
				{
					PCGEX_ASYNC_THIS
					This->FilterVtxScope(Scope);
					const TArray<PCGExCluster::FNode>& Nodes = *This->Cluster->Nodes.Get();
					for (int i = Scope.Start; i < Scope.End; i++)
					{
						if (!This->IsNodePassingFilters(Nodes[i])) { continue; }
						TSharedPtr<FDiffusion> NewDiffusion = MakeShared<FDiffusion>(This->Cluster, &Nodes[i]);
						NewDiffusion->Init();
						This->InitialDiffusions->Get(Scope)->Add(NewDiffusion);
					}
				};

			DiffusionInitialization->StartSubLoops(OngoingDiffusions.Num(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);
		}
		else
		{
			if (Settings->bUseOctreeSearch) { Cluster->RebuildOctree(Settings->SeedPicking.PickingMethod); }

			DiffusionInitialization->OnSubLoopStartCallback =
				[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
				{
					PCGEX_ASYNC_THIS
					const TArray<FPCGPoint>& Seeds = This->Context->SeedsDataFacade->Source->GetPoints(PCGExData::ESource::In);
					const TArray<PCGExCluster::FNode>& Nodes = *This->Cluster->Nodes.Get();
					for (int i = Scope.Start; i < Scope.End; i++)
					{
						FVector SeedLocation = Seeds[i].Transform.GetLocation();
						const int32 ClosestIndex = This->Cluster->FindClosestNode(SeedLocation, This->Settings->SeedPicking.PickingMethod);

						if (ClosestIndex < 0) { continue; }

						const PCGExCluster::FNode* SeedNode = &Nodes[ClosestIndex];

						if (!This->Settings->SeedPicking.WithinDistance(This->Cluster->GetPos(SeedNode), SeedLocation)) { continue; }

						TSharedPtr<FDiffusion> NewDiffusion = MakeShared<FDiffusion>(This->Cluster, SeedNode);
						NewDiffusion->SeedIndex = i;
						NewDiffusion->Init();
						This->InitialDiffusions->Get(Scope)->Add(NewDiffusion);
					}
				};

			DiffusionInitialization->StartSubLoops(OngoingDiffusions.Num(), 32);
		}

		return true;
	}

	void FProcessor::StartGrowth()
	{
		// TODO : Sort initial diffusion 

		OngoingDiffusions.Reserve(InitialDiffusions->GetTotalNum());

		InitialDiffusions->ForEach([&](const TArray<TSharedPtr<FDiffusion>>& Diffs) { OngoingDiffusions.Append(Diffs); });
		InitialDiffusions.Reset();

		Diffusions.Reserve(OngoingDiffusions.Num());

		if (Settings->Processing == EPCGExDiffusionProcessing::Parallel)
		{
			Grow();
		}
		else
		{
			PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, GrowDiffusions)
			GrowDiffusions->OnSubLoopStartCallback =
				[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
				{
					PCGEX_ASYNC_THIS
					for (int i = Scope.Start; i < Scope.End; i++) { This->Grow(); }
				};

			GrowDiffusions->StartSubLoops(OngoingDiffusions.Num(), 12, true);
		}
	}

	void FProcessor::Grow()
	{
		if (OngoingDiffusions.IsEmpty()) { return; }

		if (Settings->Processing == EPCGExDiffusionProcessing::Parallel)
		{
			StartParallelLoopForRange(OngoingDiffusions.Num());
			return;
		}

		TSharedPtr<FDiffusion> Diffusion = OngoingDiffusions.Pop();
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope)
	{
		OngoingDiffusions[Iteration]->Grow();
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		// A single growth iteration pass is complete

		const int32 OngoingNum = OngoingDiffusions.Num();

		// Move stopped diffusions in another castle

		int32 WriteIndex = 0;
		for (int32 i = 0; i < OngoingNum; i++)
		{
			const TSharedPtr<FDiffusion> Diff = OngoingDiffusions[i];
			if (Diff->bStopped) { Diffusions.Add(Diff); }
			else { OngoingDiffusions[WriteIndex++] = Diff; }
		}

		if (OngoingDiffusions.IsEmpty())
		{
			// TODO : Wrap up
			return;
		}

		// Sort current diffusions & move to the next iteration

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, SortDiffusions)
		SortDiffusions->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->Grow();
			};

		SortDiffusions->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				for (int i = Scope.Start; i < Scope.End; i++) { This->OngoingDiffusions[i]->SortCandidates(); }
			};

		SortDiffusions->StartSubLoops(OngoingDiffusions.Num(), 32);
	}

	void FProcessor::CompleteWork()
	{
		// Proceed to blending
		// Note: There is an important probability of collision for nodes with influences > 1

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, DiffuseDiffusions)

		DiffuseDiffusions->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				for (int i = Scope.Start; i < Scope.End; i++) { This->Diffusions[i]->Diffuse(); }
			};

		DiffuseDiffusions->StartSubLoops(Diffusions.Num(), 32);
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