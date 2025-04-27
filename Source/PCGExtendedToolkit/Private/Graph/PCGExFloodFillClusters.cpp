// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFloodFillClusters.h"

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

	if (Seeds.Source == EPCGExFloodFillSource::Filters)
	{
		// Add filter input
		PCGEX_PIN_FACTORIES(PCGExPointFilter::SourceVtxFiltersLabel, "Filters used to pick and choose which vtx will be used as seeds. Supports Regular & Node filters.", Required, {})

		if (Seeds.Ordering == EPCGExFloodFillOrder::Sorting)
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

	if (Settings->Seeds.Source == EPCGExFloodFillSource::Points)
	{
		Context->SeedsDataFacade = PCGExData::TryGetSingleFacade(Context, PCGExGraph::SourceSeedsLabel, true);
		if (!Context->SeedsDataFacade) { return false; }

		Context->SeedForwardHandler = Settings->SeedForwarding.GetHandler(Context->SeedsDataFacade);
	}
	else
	{
		if (!PCGExFactories::GetInputFactories<UPCGExFilterFactoryData>(
			Context, PCGExPointFilter::SourceVtxFiltersLabel, Context->FilterFactories,
			PCGExFactories::ClusterNodeFilters, true))
		{
			return false;
		}
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
				if (Settings->Seeds.Source == EPCGExFloodFillSource::Filters) { NewBatch->VtxFilterFactories = &Context->FilterFactories; }
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
	FDiffusion::FDiffusion(const TSharedPtr<FProcessor>& InProcessor, const PCGExCluster::FNode* InSeedNode)
		: Processor(InProcessor), Cluster(Processor->Cluster), SeedNode(InSeedNode)
	{
	}

	void FDiffusion::Init()
	{
		Visited.Add(SeedNode->Index);
		*(Processor->InfluencesCount->GetData() + SeedNode->PointIndex) = 1;
		FCandidate& SeedCandidate = Captured.Emplace_GetRef();
		SeedCandidate.Node = SeedNode;

		int32 SettingsIndex = SeedIndex != -1 ? SeedIndex : SeedNode->PointIndex;

		FillRate = Processor->FillRate->Read(SettingsIndex);

		CountLimit = Processor->CountLimit->Read(SettingsIndex);
		DepthLimit = Processor->DepthLimit->Read(SettingsIndex);
		DistanceLimit = Processor->DistanceLimit->Read(SettingsIndex);

		Probe(SeedCandidate);
	}

	void FDiffusion::Probe(const FCandidate& From)
	{
		if (From.Depth >= DepthLimit)
		{
			// Max depth reached
			return;
		}

		// Gather all neighbors and compute heuristics, add to candidate for the first time only
		bool bIsAlreadyInSet = false;

		const PCGExCluster::FNode& FromNode = *From.Node;
		const PCGExCluster::FNode& RoamingGoal = *Processor->HeuristicsHandler->GetRoamingGoal();

		FVector FromPosition = Cluster->GetPos(FromNode);

		for (const PCGExGraph::FLink& Lk : FromNode.Links)
		{
			PCGExCluster::FNode* OtherNode = Cluster->GetNode(Lk);
			Visited.Add(OtherNode->Index, &bIsAlreadyInSet);

			if (bIsAlreadyInSet) { continue; }

			FVector OtherPosition = Cluster->GetPos(OtherNode);
			double Dist = FVector::Dist(FromPosition, OtherPosition);

			if ((From.Distance + Dist) > DistanceLimit)
			{
				// Outside distance limit
				continue;
			}

			// TODO : Implement radius limit

			FCandidate& Candidate = Candidates.Emplace_GetRef();
			Candidate.Node = OtherNode;


			if (Processor->bUseLocalScore || Processor->bUsePreviousScore)
			{
				const double LocalScore = Processor->HeuristicsHandler->GetEdgeScore(
					FromNode, *OtherNode,
					*Cluster->GetEdge(Lk), *SeedNode, RoamingGoal,
					nullptr, TravelStack);

				if (Processor->bUsePreviousScore)
				{
					Candidate.PathScore = From.PathScore + LocalScore;
					Candidate.Score += From.PathScore;
				}

				if (Processor->bUseLocalScore)
				{
					Candidate.Score += LocalScore;
				}
			}

			if (Processor->bUseGlobalScore) { Candidate.Score += Processor->HeuristicsHandler->GetGlobalScore(FromNode, *SeedNode, *OtherNode); }

			Candidate.Depth = From.Depth + 1;
			Candidate.Distance = From.Distance + Dist; // TODO : Compute distance
		}
	}

	void FDiffusion::Grow()
	{
		if (bStopped) { return; }

		bool bSearch = true;
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

			int8 Influences = FPlatformAtomics::AtomicRead((Processor->InfluencesCount->GetData() + Candidate.Node->PointIndex));
			if (Influences >= 1) { continue; } // Validate candidate is still valid

			FPlatformAtomics::InterlockedIncrement((Processor->InfluencesCount->GetData() + Candidate.Node->PointIndex));

			// Update max depth & max distance
			MaxDepth = FMath::Max(MaxDepth, Candidate.Depth);
			MaxDistance = FMath::Max(MaxDistance, Candidate.Distance);

			Captured.Add(Candidate);
			PostGrow();

			bSearch = false;

			if (Captured.Num() >= CountLimit)
			{
				// Max Count reached
				bStopped = true;
				break;
			}
		}
	}

	void FDiffusion::PostGrow()
	{
		// Probe from last captured candidate

		Probe(Captured.Last());

		// Sort candidates

		switch (Processor->Settings->Diffusion.Priority)
		{
		case EPCGExDiffusionPrioritization::Heuristics:
			Candidates.Sort(
				[&](const FCandidate& A, const FCandidate& B)
				{
					if (A.Score == B.Score) { return A.Depth > B.Depth; }
					return A.Score > B.Score;
				});
			break;
		case EPCGExDiffusionPrioritization::Depth:
			Candidates.Sort(
				[&](const FCandidate& A, const FCandidate& B)
				{
					if (A.Depth == B.Depth) { return A.Score > B.Score; }
					return A.Depth > B.Depth;
				});
			break;
		}
	}

	void FDiffusion::Diffuse()
	{
		TArray<UPCGExAttributeBlendOperation*>& Operations = *Processor->Operations.Get();

		TArray<int32> Indices;
		const TArray<FPCGPoint>& InPoints = Processor->VtxDataFacade->Source->GetPoints(PCGExData::ESource::In);
		TArray<FPCGPoint>& OutPoints = Processor->VtxDataFacade->Source->GetMutablePoints();

		Indices.SetNumUninitialized(Captured.Num());

		const int32 SourceIndex = SeedNode->PointIndex;
		const FPCGPoint& SourcePoint = InPoints[SourceIndex];

		for (int i = 0; i < Indices.Num(); i++)
		{
			const FCandidate& Candidate = Captured[i];
			const int32 TargetIndex = Candidate.Node->PointIndex;

			Indices[i] = TargetIndex;

			if (TargetIndex != SourceIndex)
			{
				FPCGPoint& TargetPoint = OutPoints[TargetIndex];

				// TODO : Compute weight based on distance or depth

				for (UPCGExAttributeBlendOperation* Op : Operations)
				{
					Op->Blend(SourceIndex, SourcePoint, TargetIndex, TargetPoint);
				}
			}

			if (Processor->DiffusionDepthWriter) { Processor->DiffusionDepthWriter->GetMutable(TargetIndex) = Candidate.Depth; }
			if (Processor->DiffusionDistanceWriter) { Processor->DiffusionDistanceWriter->GetMutable(TargetIndex) = Candidate.Distance; }
			if (Processor->DiffusionOrderWriter) { Processor->DiffusionOrderWriter->GetMutable(TargetIndex) = i; }
		}

		if (SeedIndex != -1)
		{
			Processor->Context->SeedForwardHandler->Forward(SeedIndex, Processor->VtxDataFacade, Indices);
		}
	}

	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExClusterDiffusion::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		bUseLocalScore = (Settings->Diffusion.Scoring & static_cast<uint8>(EPCGExDiffusionHeuristicFlags::LocalScore)) != 0;
		bUseGlobalScore = (Settings->Diffusion.Scoring & static_cast<uint8>(EPCGExDiffusionHeuristicFlags::GlobalScore)) != 0;
		bUsePreviousScore = (Settings->Diffusion.Scoring & static_cast<uint8>(EPCGExDiffusionHeuristicFlags::PreviousScore)) != 0;

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

		int32 InitIterations = 0;

		if (Settings->Seeds.Source == EPCGExFloodFillSource::Filters)
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
						TSharedPtr<FDiffusion> NewDiffusion = MakeShared<FDiffusion>(This, &Nodes[i]);
						NewDiffusion->Init();
						This->InitialDiffusions->Get(Scope)->Add(NewDiffusion);
					}
				};

			InitIterations = Cluster->Nodes->Num();
		}
		else
		{
			if (Settings->bUseOctreeSearch) { Cluster->RebuildOctree(Settings->Seeds.SeedPicking.PickingMethod); }

			DiffusionInitialization->OnSubLoopStartCallback =
				[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
				{
					PCGEX_ASYNC_THIS
					const TArray<FPCGPoint>& Seeds = This->Context->SeedsDataFacade->Source->GetPoints(PCGExData::ESource::In);
					const TArray<PCGExCluster::FNode>& Nodes = *This->Cluster->Nodes.Get();
					for (int i = Scope.Start; i < Scope.End; i++)
					{
						FVector SeedLocation = Seeds[i].Transform.GetLocation();
						const int32 ClosestIndex = This->Cluster->FindClosestNode(SeedLocation, This->Settings->Seeds.SeedPicking.PickingMethod);

						if (ClosestIndex < 0) { continue; }

						const PCGExCluster::FNode* SeedNode = &Nodes[ClosestIndex];

						if (!This->Settings->Seeds.SeedPicking.WithinDistance(This->Cluster->GetPos(SeedNode), SeedLocation)) { continue; }

						TSharedPtr<FDiffusion> NewDiffusion = MakeShared<FDiffusion>(This, SeedNode);
						NewDiffusion->SeedIndex = i;
						NewDiffusion->Init();
						This->InitialDiffusions->Get(Scope)->Add(NewDiffusion);
					}
				};

			InitIterations = Context->SeedsDataFacade->GetNum();
		}

		if (InitIterations <= 0) { return false; }

		DiffusionInitialization->StartSubLoops(InitIterations, GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);

		return true;
	}

	void FProcessor::StartGrowth()
	{
		InitialDiffusions->Collapse(OngoingDiffusions);
		InitialDiffusions.Reset();

		if (OngoingDiffusions.IsEmpty())
		{
			// TODO : Warn that no diffusion could be initialized
			bIsProcessorValid = false;
			return;
		}

		// TODO : Sort OngoingDiffusions diffusion once

		Diffusions.Reserve(OngoingDiffusions.Num());

		if (Settings->Diffusion.Processing == EPCGExFloodFillProcessing::Parallel)
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

			GrowDiffusions->StartSubLoops(OngoingDiffusions.Num(), 1);
		}
	}

	void FProcessor::Grow()
	{
		if (OngoingDiffusions.IsEmpty()) { return; }

		if (Settings->Diffusion.Processing == EPCGExFloodFillProcessing::Parallel)
		{
			// Grow all by a single step
			StartParallelLoopForRange(OngoingDiffusions.Num());
			return;
		}

		// Grow one entierely
		TSharedPtr<FDiffusion> Diffusion = OngoingDiffusions.Pop();
		while (!Diffusion->bStopped) { Diffusion->Grow(); }

		Diffusions.Add(Diffusion);

		Grow(); // Move to the next
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope)
	{
		TSharedPtr<FDiffusion> Diffusion = OngoingDiffusions[Iteration];
		for (int i = 0; i < Diffusion->FillRate; i++) { Diffusion->Grow(); }
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

		OngoingDiffusions.SetNum(WriteIndex);

		if (OngoingDiffusions.IsEmpty()) { return; }

		Grow();
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

		DiffuseDiffusions->StartSubLoops(Diffusions.Num(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
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

		if (Settings->Seeds.Source == EPCGExFloodFillSource::Filters)
		{
			if (Settings->Diffusion.FillRateInput == EPCGExInputValueType::Attribute)
			{
				FacadePreloader.Register<int32>(Context, Settings->Diffusion.FillRateAttribute);
			}

#define PCGEX_DIFFUSION_REGISTER_LIMIT(_NAME) if (Settings->bUse##_NAME && Settings->_NAME##Input == EPCGExInputValueType::Attribute) \
{ FacadePreloader.Register<int32>(Context, Settings->_NAME##Attribute); }

			PCGEX_DIFFUSION_REGISTER_LIMIT(MaxCount)
			PCGEX_DIFFUSION_REGISTER_LIMIT(MaxDepth)
			PCGEX_DIFFUSION_REGISTER_LIMIT(MaxLength)

#undef PCGEX_DIFFUSION_REGISTER_LIMIT
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

		InfluencesCount = MakeShared<TArray<int8>>();
		InfluencesCount->Init(0, VtxDataFacade->GetNum());

		TSharedPtr<PCGExData::FFacade> SettingsSource = Settings->Seeds.Source == EPCGExFloodFillSource::Filters ? VtxDataFacade : Context->SeedsDataFacade;

		// Diffusion rate

		FillRate = PCGExDetails::MakeSettingValue<int32>(Settings->Diffusion.FillRateInput, Settings->Diffusion.FillRateAttribute, Settings->Diffusion.FillRateConstant);
		bIsBatchValid = FillRate->Init(Context, SettingsSource);

		if (Settings->bUseMaxCount)
		{
			CountLimit = PCGExDetails::MakeSettingValue<int32>(Settings->MaxCountInput, Settings->MaxCountAttribute, Settings->MaxCount);
			bIsBatchValid = CountLimit->Init(Context, SettingsSource);
		}
		else
		{
			CountLimit = PCGExDetails::MakeSettingValue<int32>(MAX_int32);
		}

		if (Settings->bUseMaxDepth)
		{
			DepthLimit = PCGExDetails::MakeSettingValue<int32>(Settings->MaxDepthInput, Settings->MaxDepthAttribute, Settings->MaxDepth);
			bIsBatchValid = DepthLimit->Init(Context, SettingsSource);
			
		}
		else
		{
			DepthLimit = PCGExDetails::MakeSettingValue<int32>(MAX_int32);
		}

		if (Settings->bUseMaxLength)
		{
			DistanceLimit = PCGExDetails::MakeSettingValue<double>(Settings->MaxLengthInput, Settings->MaxLengthAttribute, Settings->MaxLength);
			bIsBatchValid = DistanceLimit->Init(Context, SettingsSource);
		}
		else
		{
			DistanceLimit = PCGExDetails::MakeSettingValue<double>(MAX_dbl);
		}

		if (!bIsBatchValid) { return; } // Fail

		TBatchWithHeuristics<FProcessor>::Process();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor)
	{
		if (!TBatch<FProcessor>::PrepareSingle(ClusterProcessor)) { return false; }

		ClusterProcessor->Operations = Operations;
		ClusterProcessor->InfluencesCount = InfluencesCount;

		ClusterProcessor->FillRate = FillRate;

		ClusterProcessor->CountLimit = CountLimit;
		ClusterProcessor->DepthLimit = DepthLimit;
		ClusterProcessor->DistanceLimit = DistanceLimit;

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
