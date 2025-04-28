// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/FloodFill/PCGExFloodFillClusters.h"

#include "GeometryCollection/Facades/CollectionConstraintOverrideFacade.h"
#include "Graph/FloodFill/PCGExFloodFill.h"
#include "Graph/FloodFill/FillControls/PCGExFillControlsFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExClusterDiffusion"
#define PCGEX_NAMESPACE ClusterDiffusion

PCGExData::EIOInit UPCGExClusterDiffusionSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }
PCGExData::EIOInit UPCGExClusterDiffusionSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Forward; }

TArray<FPCGPinProperties> UPCGExClusterDiffusionSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	PCGEX_PIN_FACTORIES(PCGExGraph::SourceHeuristicsLabel, "Heuristics. Used to drive flooding.", Required, {})
	PCGEX_PIN_POINT(PCGExGraph::SourceSeedsLabel, "Seed points.", Required, {})
	PCGEX_PIN_FACTORIES(PCGExFloodFill::SourceFillControlsLabel, "Fill controls, used to constraint & limit flood fill", Normal, {})
	PCGEX_PIN_FACTORIES(PCGExDataBlending::SourceBlendingLabel, "Blending configurations.", Normal, {})

	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(ClusterDiffusion)

bool FPCGExClusterDiffusionElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ClusterDiffusion)
	PCGEX_FOREACH_FIELD_CLUSTER_DIFF(PCGEX_OUTPUT_VALIDATE_NAME)

	PCGExFactories::GetInputFactories<UPCGExAttributeBlendFactory>(
		Context, PCGExDataBlending::SourceBlendingLabel, Context->BlendingFactories,
		{PCGExFactories::EType::Blending}, false);

	// Fill controls are optional, actually
	PCGExFactories::GetInputFactories<UPCGExFillControlsFactoryData>(
		Context, PCGExFloodFill::SourceFillControlsLabel, Context->FillControlFactories,
		{PCGExFactories::EType::FillControls}, false);

	Context->SeedsDataFacade = PCGExData::TryGetSingleFacade(Context, PCGExGraph::SourceSeedsLabel, true);
	if (!Context->SeedsDataFacade) { return false; }

	Context->SeedForwardHandler = Settings->SeedForwarding.GetHandler(Context->SeedsDataFacade);

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
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExClusterDiffusion::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		FillControlsHandler = MakeShared<PCGExFloodFill::FFillControlsHandler>(
			Context, Cluster,
			VtxDataFacade, EdgeDataFacade, Context->SeedsDataFacade,
			Context->FillControlFactories);

		FillControlsHandler->HeuristicsHandler = HeuristicsHandler;
		FillControlsHandler->InfluencesCount = InfluencesCount;

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
				This->InitialDiffusions = MakeShared<PCGExMT::TScopedArray<TSharedPtr<PCGExFloodFill::FDiffusion>>>(Loops);
			};

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

					TSharedPtr<PCGExFloodFill::FDiffusion> NewDiffusion = MakeShared<PCGExFloodFill::FDiffusion>(This->FillControlsHandler, This->Cluster, SeedNode);
					NewDiffusion->Index = i;
					This->InitialDiffusions->Get(Scope)->Add(NewDiffusion);
				}
			};

		if (Context->SeedsDataFacade->GetNum() <= 0) { return false; }

		DiffusionInitialization->StartSubLoops(Context->SeedsDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->ClusterDefaultBatchChunkSize);

#undef PCGEX_NEW_DIFFUSION

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

		// Prepare control handler before initializing diffusion
		// since the init does a first probing pass
		FillControlsHandler->PrepareForDiffusions(OngoingDiffusions, Settings->Diffusion);

		for (int i = 0; i < OngoingDiffusions.Num(); i++)
		{
			TSharedPtr<PCGExFloodFill::FDiffusion> Diffusion = OngoingDiffusions[i];
			const int32 InitIndex = Diffusion->Index;
			Diffusion->Index = i;
			Diffusion->Init(InitIndex);
		}

		Diffusions.Reserve(OngoingDiffusions.Num());

		if (Settings->Processing == EPCGExFloodFillProcessing::Parallel)
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

		if (Settings->Processing == EPCGExFloodFillProcessing::Parallel)
		{
			// Grow all by a single step
			StartParallelLoopForRange(OngoingDiffusions.Num());
			return;
		}

		// Grow one entierely
		const TSharedPtr<PCGExFloodFill::FDiffusion> Diffusion = OngoingDiffusions.Pop();
		while (!Diffusion->bStopped) { Diffusion->Grow(); }

		Diffusions.Add(Diffusion);

		Grow(); // Move to the next
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope)
	{
		const TSharedPtr<PCGExFloodFill::FDiffusion> Diffusion = OngoingDiffusions[Iteration];
		const int32 CurrentFillRate = FillRate->Read(Diffusion->GetSettingsIndex(Settings->Diffusion.FillRateSource));
		for (int i = 0; i < CurrentFillRate; i++) { Diffusion->Grow(); }
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		// A single growth iteration pass is complete
		const int32 OngoingNum = OngoingDiffusions.Num();

		// Move stopped diffusions in another castle
		int32 WriteIndex = 0;
		for (int32 i = 0; i < OngoingNum; i++)
		{
			const TSharedPtr<PCGExFloodFill::FDiffusion> Diff = OngoingDiffusions[i];
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

		DiffuseDiffusions->OnIterationCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const int32 Index, const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				This->Diffuse(This->Diffusions[Index]);
			};

		DiffuseDiffusions->StartIterations(Diffusions.Num(), 1);
	}

	void FProcessor::Diffuse(const TSharedPtr<PCGExFloodFill::FDiffusion>& Diffusion) const
	{
		TArray<int32> Indices;

		// Diffuse & blend
		Diffusion->Diffuse(VtxDataFacade, *Operations.Get(), Indices);

		// Outputs
		if (!Indices.IsEmpty())
		{
			for (int i = 0; i < Indices.Num(); i++)
			{
				const PCGExFloodFill::FCandidate& Candidate = Diffusion->Captured[i];
				const int32 TargetIndex = Indices[i];

				PCGEX_OUTPUT_VALUE(DiffusionDepth, TargetIndex, Candidate.Depth);
				PCGEX_OUTPUT_VALUE(DiffusionDistance, TargetIndex, Candidate.PathDistance);
				PCGEX_OUTPUT_VALUE(DiffusionOrder, TargetIndex, i);
				//PCGEX_OUTPUT_VALUE(DiffusionEnding, TargetIndex, Diffusion->TravelStack->Get(Candidate.Node));
			}

			// Forward seed values to diffusion
			if (Diffusion->SeedIndex != -1) { Context->SeedForwardHandler->Forward(Diffusion->SeedIndex, VtxDataFacade, Indices); }
		}
	}

	void FProcessor::Cleanup()
	{
		// Make sure we flush these ASAP
		InitialDiffusions.Reset();
		OngoingDiffusions.Reset();
		Diffusions.Reset();
		FillControlsHandler.Reset();
		TProcessor<FPCGExClusterDiffusionContext, UPCGExClusterDiffusionSettings>::Cleanup();
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

		for (const TObjectPtr<const UPCGExFillControlsFactoryData>& Factory : Context->FillControlFactories)
		{
			Factory->RegisterBuffersDependencies(Context, FacadePreloader);
			// TODO : Might need to fill-in facade here as well
		}
	}

	void FBatch::Process()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ClusterDiffusion)

		BlendOps = MakeShared<TArray<UPCGExAttributeBlendOperation*>>();
		BlendOps->Reserve(Context->BlendingFactories.Num());

		for (const TObjectPtr<const UPCGExAttributeBlendFactory>& Factory : Context->BlendingFactories)
		{
			UPCGExAttributeBlendOperation* Op = Factory->CreateOperation(Context);
			if (!Op)
			{
				bIsBatchValid = false;
				PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("An operation could not be created."));
				return; // FAIL
			}

			Op->OpIdx = BlendOps->Add(Op);
			Op->SiblingOperations = BlendOps;

			if (!Op->PrepareForData(Context, VtxDataFacade))
			{
				bIsBatchValid = false;
				return; // FAIL
			}
		}

		InfluencesCount = MakeShared<TArray<int8>>();
		InfluencesCount->Init(0, VtxDataFacade->GetNum());

		// Diffusion rate

		FillRate = PCGExDetails::MakeSettingValue<int32>(Settings->Diffusion.FillRateInput, Settings->Diffusion.FillRateAttribute, Settings->Diffusion.FillRateConstant);
		bIsBatchValid = FillRate->Init(Context, Settings->Diffusion.FillRateSource == EPCGExFloodFillSettingSource::Seed ? Context->SeedsDataFacade : VtxDataFacade);
		
		if (!bIsBatchValid) { return; } // Fail

		TBatchWithHeuristics<FProcessor>::Process();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor)
	{
		if (!TBatch<FProcessor>::PrepareSingle(ClusterProcessor)) { return false; }

		ClusterProcessor->Operations = BlendOps;
		ClusterProcessor->InfluencesCount = InfluencesCount;

		ClusterProcessor->FillRate = FillRate;

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
