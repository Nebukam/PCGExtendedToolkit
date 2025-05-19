// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/FloodFill/PCGExFloodFillClusters.h"

#include "GeometryCollection/Facades/CollectionConstraintOverrideFacade.h"
#include "Graph/FloodFill/PCGExFloodFill.h"
#include "Graph/FloodFill/FillControls/PCGExFillControlsFactoryProvider.h"
#include "Paths/PCGExPaths.h"

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

TArray<FPCGPinProperties> UPCGExClusterDiffusionSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();

	if (bOutputPaths)
	{
		PCGEX_PIN_POINTS(PCGExPaths::OutputPathsLabel, "High density, overlapping paths representing individual flood lanes", Normal, {})
	}

	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(ClusterDiffusion)

bool FPCGExClusterDiffusionElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ClusterDiffusion)
	PCGEX_FOREACH_FIELD_CLUSTER_DIFF(PCGEX_OUTPUT_VALIDATE_NAME)

	PCGExFactories::GetInputFactories<UPCGExBlendOpFactory>(
		Context, PCGExDataBlending::SourceBlendingLabel, Context->BlendingFactories,
		{PCGExFactories::EType::Blending}, false);

	// Fill controls are optional, actually
	PCGExFactories::GetInputFactories<UPCGExFillControlsFactoryData>(
		Context, PCGExFloodFill::SourceFillControlsLabel, Context->FillControlFactories,
		{PCGExFactories::EType::FillControls}, false);

	Context->SeedsDataFacade = PCGExData::TryGetSingleFacade(Context, PCGExGraph::SourceSeedsLabel, false, true);
	if (!Context->SeedsDataFacade) { return false; }

	if (Settings->bOutputPaths)
	{
		PCGEX_FWD(SeedAttributesToPathTags)
		if (!Context->SeedAttributesToPathTags.Init(Context, Context->SeedsDataFacade)) { return false; }

		Context->Paths = MakeShared<PCGExData::FPointIOCollection>(Context);
		Context->Paths->OutputPin = PCGExPaths::OutputPathsLabel;
	}

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

	if (Settings->bOutputPaths)
	{
		PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_ReadyForNextPoints)

		if (Context->ExpectedPathCount > 0)
		{
			PCGEX_ON_STATE(PCGEx::State_ReadyForNextPoints)
			{
				Context->SetAsyncState(PCGEx::State_WaitingOnAsyncWork);
				Context->OutputBatches();
			}

			PCGEX_ON_ASYNC_STATE_READY(PCGEx::State_WaitingOnAsyncWork)
			{
				Context->Paths->StageOutputs();
				Context->Done();
			}
		}
		else
		{
			// No point, maybe give a warning or something?
			Context->Done();
		}
	}
	else
	{
		PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)
	}

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

		Seeded.Init(0, Cluster->Nodes->Num());

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
				const TArray<FPCGPoint>& Seeds = This->Context->SeedsDataFacade->Source->GetPoints(PCGExData::EIOSide::In);
				const TArray<PCGExCluster::FNode>& Nodes = *This->Cluster->Nodes.Get();
				PCGEX_SCOPE_LOOP(i)
				{
					FVector SeedLocation = Seeds[i].Transform.GetLocation();
					const int32 ClosestIndex = This->Cluster->FindClosestNode(SeedLocation, This->Settings->Seeds.SeedPicking.PickingMethod);

					if (ClosestIndex < 0 ||
						FPlatformAtomics::InterlockedCompareExchange(&This->Seeded[ClosestIndex], 1, 0) == 1)
					{
						continue;
					}

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
		Seeded.Empty();

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
		if (!FillControlsHandler->PrepareForDiffusions(OngoingDiffusions, Settings->Diffusion))
		{
			bIsProcessorValid = false;
			return;
		}

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
					PCGEX_SCOPE_LOOP(i) { This->Grow(); }
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

	void FProcessor::Diffuse(const TSharedPtr<PCGExFloodFill::FDiffusion>& Diffusion)
	{
		TArray<int32> Indices;

		// Diffuse & blend
		Diffusion->Diffuse(VtxDataFacade, BlendOpsManager, Indices);
		FPlatformAtomics::InterlockedAdd(&ExpectedPathCount, Diffusion->Endpoints.Num());
		FPlatformAtomics::InterlockedAdd(&Context->ExpectedPathCount, ExpectedPathCount);

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
				PCGEX_OUTPUT_VALUE(DiffusionEnding, TargetIndex, Diffusion->Endpoints.Contains(Candidate.Node->Index));
			}

			// Forward seed values to diffusion
			if (Diffusion->SeedIndex != -1) { Context->SeedForwardHandler->Forward(Diffusion->SeedIndex, VtxDataFacade, Indices); }
		}

		Diffusion->Captured.Empty();
		Diffusion->Candidates.Empty();

		// TODO : Cleanup the diffusion if we don't want paths
	}

	void FProcessor::Output()
	{
		if (ExpectedPathCount == 0) { return; }
		Context->ExpectedPathCount += ExpectedPathCount;

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, PathsTaskGroup)
		PathsTaskGroup->OnIterationCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const int32 Index, const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				TSharedPtr<PCGExFloodFill::FDiffusion> Diff = This->Diffusions[Index];
				for (const int32 EndpointIndex : Diff->Endpoints) { This->WritePath(Index, EndpointIndex); }
			};

		PathsTaskGroup->StartIterations(Diffusions.Num(), 1);
	}

	void FProcessor::WritePath(const int32 DiffusionIndex, const int32 EndpointNodeIndex)
	{
		TSharedPtr<PCGExFloodFill::FDiffusion> Diffusion = Diffusions[DiffusionIndex];
		const int32 SeedNodeIndex = Diffusion->SeedNode->Index;

		int32 PathNodeIndex = PCGEx::NH64A(Diffusion->TravelStack->Get(EndpointNodeIndex));
		int32 PathEdgeIndex = -1;

		TArray<int32> PathIndices;
		if (PathNodeIndex != -1)
		{
			PathIndices.Add(Cluster->GetNode(EndpointNodeIndex)->PointIndex);

			while (PathNodeIndex != -1)
			{
				const int32 CurrentIndex = PathNodeIndex;
				PCGEx::NH64(Diffusion->TravelStack->Get(CurrentIndex), PathNodeIndex, PathEdgeIndex);
				PathIndices.Add(Cluster->GetNode(CurrentIndex)->PointIndex);
			}
		}

		if (PathIndices.Num() < 2) { return; }

		Algo::Reverse(PathIndices);

		// Create a copy of the final vtx, so we get all the goodies

		TSharedPtr<PCGExData::FPointIO> PathIO = Context->Paths->Emplace_GetRef(VtxDataFacade->Source->GetOut(), PCGExData::EIOInit::New);
		const TArray<FPCGPoint>& VtxPoints = VtxDataFacade->Source->GetOut()->GetPoints();
		TArray<FPCGPoint>& MutablePoints = PathIO->GetOut()->GetMutablePoints();

		MutablePoints.SetNumUninitialized(PathIndices.Num());
		for (int i = 0; i < PathIndices.Num(); i++) { MutablePoints[i] = VtxPoints[PathIndices[i]]; }

		Context->SeedAttributesToPathTags.Tag(Diffusion->SeedIndex, PathIO);

		PathIO->IOIndex = Diffusion->SeedIndex * 1000000 + VtxDataFacade->Source->IOIndex * 1000000 + EndpointNodeIndex;
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExClusterDiffusionContext, UPCGExClusterDiffusionSettings>::Cleanup();

		// Make sure we flush these ASAP
		InitialDiffusions.Reset();
		OngoingDiffusions.Reset();
		Diffusions.Reset();
		FillControlsHandler.Reset();
		BlendOpsManager.Reset();
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

		PCGExDataBlending::RegisterBuffersDependencies(Context, FacadePreloader, Context->BlendingFactories);

		for (const TObjectPtr<const UPCGExFillControlsFactoryData>& Factory : Context->FillControlFactories)
		{
			Factory->RegisterBuffersDependencies(Context, FacadePreloader);
			// TODO : Might need to fill-in facade here as well
		}
	}

	void FBatch::Process()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ClusterDiffusion)

		BlendOpsManager = MakeShared<PCGExDataBlending::FBlendOpsManager>(VtxDataFacade);
		if(!BlendOpsManager->Init(Context, Context->BlendingFactories)){
			bIsBatchValid = false;
			return;
		}

		InfluencesCount = MakeShared<TArray<int8>>();
		InfluencesCount->Init(-1, VtxDataFacade->GetNum());

		// Diffusion rate

		FillRate = PCGExDetails::MakeSettingValue<int32>(Settings->Diffusion.FillRateInput, Settings->Diffusion.FillRateAttribute, Settings->Diffusion.FillRateConstant);
		bIsBatchValid = FillRate->Init(Context, Settings->Diffusion.FillRateSource == EPCGExFloodFillSettingSource::Seed ? Context->SeedsDataFacade : VtxDataFacade);

		if (!bIsBatchValid) { return; } // Fail

		TBatchWithHeuristics<FProcessor>::Process();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor)
	{
		if (!TBatch<FProcessor>::PrepareSingle(ClusterProcessor)) { return false; }

		ClusterProcessor->BlendOpsManager = BlendOpsManager;
		ClusterProcessor->InfluencesCount = InfluencesCount;

		ClusterProcessor->FillRate = FillRate;

#define PCGEX_OUTPUT_FWD_TO(_NAME, _TYPE, _DEFAULT_VALUE) if(_NAME##Writer){ ClusterProcessor->_NAME##Writer = _NAME##Writer; }
		PCGEX_FOREACH_FIELD_CLUSTER_DIFF(PCGEX_OUTPUT_FWD_TO)
#undef PCGEX_OUTPUT_FWD_TO

		return true;
	}

	void FBatch::Write()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ClusterDiffusion)
		
		TBatch<FProcessor>::Write();
		BlendOpsManager->Cleanup(Context);
		VtxDataFacade->Write(AsyncManager);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
