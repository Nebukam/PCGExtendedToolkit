// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingFindContours.h"

#include "PCGExCompare.h"


#define LOCTEXT_NAMESPACE "PCGExFindContours"
#define PCGEX_NAMESPACE FindContours

TArray<FPCGPinProperties> UPCGExFindContoursSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGExGraph::SourceSeedsLabel, "Seeds associated with the main input points", Required, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExFindContoursSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExPaths::OutputPathsLabel, "Contours", Required, {})
	if (bOutputFilteredSeeds)
	{
		PCGEX_PIN_POINT(PCGExFindContours::OutputGoodSeedsLabel, "GoodSeeds", Required, {})
		PCGEX_PIN_POINT(PCGExFindContours::OutputBadSeedsLabel, "BadSeeds", Required, {})
	}
	return PinProperties;
}

PCGExData::EIOInit UPCGExFindContoursSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::None; }
PCGExData::EIOInit UPCGExFindContoursSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

PCGEX_INITIALIZE_ELEMENT(FindContours)

bool FPCGExFindContoursElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindContours)

	PCGEX_FWD(ProjectionDetails)
	PCGEX_FWD(Artifacts)
	if (!Context->Artifacts.Init(Context)) { return false; }

	Context->SeedsDataFacade = PCGExData::TryGetSingleFacade(Context, PCGExGraph::SourceSeedsLabel, false, true);
	if (!Context->SeedsDataFacade) { return false; }

	if (!Context->ProjectionDetails.Init(Context, Context->SeedsDataFacade)) { return false; }

	PCGEX_FWD(SeedAttributesToPathTags)
	if (!Context->SeedAttributesToPathTags.Init(Context, Context->SeedsDataFacade)) { return false; }
	Context->SeedForwardHandler = Settings->SeedForwarding.GetHandler(Context->SeedsDataFacade);

	Context->Paths = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->Paths->OutputPin = PCGExPaths::OutputPathsLabel;

	if (Settings->bOutputFilteredSeeds)
	{
		const int32 NumSeeds = Context->SeedsDataFacade->GetNum();

		Context->SeedQuality.Init(false, NumSeeds);
		PCGEx::InitArray(Context->UdpatedSeedPoints, NumSeeds);

		Context->GoodSeeds = NewPointIO(Context->SeedsDataFacade->Source, PCGExFindContours::OutputGoodSeedsLabel);
		Context->GoodSeeds->InitializeOutput(PCGExData::EIOInit::Duplicate);
		PCGEx::SetNumPointsAllocated(Context->GoodSeeds->GetOut(), NumSeeds);

		Context->BadSeeds = NewPointIO(Context->SeedsDataFacade->Source, PCGExFindContours::OutputBadSeedsLabel);
		Context->BadSeeds->InitializeOutput(PCGExData::EIOInit::Duplicate);
		PCGEx::SetNumPointsAllocated(Context->BadSeeds->GetOut(), NumSeeds);
	}

	return true;
}

bool FPCGExFindContoursElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindContoursElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FindContours)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExFindContours::FBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExFindContours::FBatch>& NewBatch)
			{
				//NewBatch->bRequiresWriteStep = Settings->Artifacts.WriteAny();
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	if (Settings->bOutputFilteredSeeds)
	{
		Context->GoodSeeds->Gather(Context->SeedQuality);
		Context->BadSeeds->Gather(Context->SeedQuality, true);

		(void)Context->GoodSeeds->StageOutput(Context);
		(void)Context->BadSeeds->StageOutput(Context);
	}

	Context->Paths->StageOutputs();

	return Context->TryComplete();
}


namespace PCGExFindContours
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFindContours::Process);


		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		if (Settings->bUseOctreeSearch) { Cluster->RebuildOctree(Settings->SeedPicking.PickingMethod); }
		Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Edge); // We need edge octree anyway

		CellsConstraints = MakeShared<PCGExTopology::FCellConstraints>(Settings->Constraints);
		if (Settings->Constraints.bOmitWrappingBounds) { CellsConstraints->BuildWrapperCell(Cluster.ToSharedRef(), *ProjectedPositions); }

		StartParallelLoopForRange(Context->SeedsDataFacade->Source->GetNum(), 64);

		return true;
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		TConstPCGValueRange<FTransform> InSeedTransforms = Context->SeedsDataFacade->GetIn()->GetConstTransformValueRange();

		PCGEX_SCOPE_LOOP(Index)
		{
			const FVector SeedWP = InSeedTransforms[Index].GetLocation();

			const TSharedPtr<PCGExTopology::FCell> Cell = MakeShared<PCGExTopology::FCell>(CellsConstraints.ToSharedRef());
			const PCGExTopology::ECellResult Result = Cell->BuildFromCluster(SeedWP, Cluster.ToSharedRef(), *ProjectedPositions, &Settings->SeedPicking);

			if (Result != PCGExTopology::ECellResult::Success)
			{
				if (Result == PCGExTopology::ECellResult::WrapperCell ||
					(CellsConstraints->WrapperCell && CellsConstraints->WrapperCell->GetCellHash() == Cell->GetCellHash()))
				{
					// Only track the seed closest to bound center as being associated with the wrapper.
					// There may be edge cases where we don't want that to happen

					const double DistToSeed = FVector::DistSquared(SeedWP, Cluster->Bounds.GetCenter());
					{
						FReadScopeLock ReadScopeLock(WrappedSeedLock);
						if (ClosestSeedDist < DistToSeed) { continue; }
					}
					{
						FReadScopeLock WriteScopeLock(WrappedSeedLock);
						if (ClosestSeedDist < DistToSeed) { continue; }
						ClosestSeedDist = DistToSeed;
						WrapperSeed = Index;
					}
				}
				continue;
			}

			ProcessCell(Index, Cell);
		}
	}

	void FProcessor::ProcessCell(const int32 SeedIndex, const TSharedPtr<PCGExTopology::FCell>& InCell)
	{
		TSharedPtr<PCGExData::FPointIO> PathIO = Context->Paths->Emplace_GetRef<UPCGPointArrayData>(VtxDataFacade->Source, PCGExData::EIOInit::New);
		if (!PathIO) { return; }

		const int32 NumCellPoints = InCell->Nodes.Num();
		PCGEx::SetNumPointsAllocated(PathIO->GetOut(), NumCellPoints);

		PathIO->Tags->Reset();                              // Tag forwarding handled by artifacts
		PathIO->IOIndex = BatchIndex * 1000000 + SeedIndex; // Enforce seed order for collection output

		PCGExGraph::CleanupClusterTags(PathIO);
		PCGExGraph::CleanupVtxData(PathIO);

		PCGEX_MAKE_SHARED(PathDataFacade, PCGExData::FFacade, PathIO.ToSharedRef())

		TArray<int32> ReadIndices;
		ReadIndices.SetNumUninitialized(NumCellPoints);

		for (int i = 0; i < NumCellPoints; i++) { ReadIndices[i] = Cluster->GetNode(InCell->Nodes[i])->PointIndex; }

		PathIO->InheritPoints(ReadIndices, 0);
		InCell->PostProcessPoints(PathIO->GetOut());

		Context->SeedAttributesToPathTags.Tag(Context->SeedsDataFacade->GetInPoint(SeedIndex), PathIO);
		Context->SeedForwardHandler->Forward(SeedIndex, PathDataFacade);

		Context->Artifacts.Process(Cluster, PathDataFacade, InCell);
		PathDataFacade->Write(AsyncManager);

		if (Settings->bOutputFilteredSeeds)
		{
			Context->SeedQuality[SeedIndex] = true;
			PCGExData::FMutablePoint SeedPoint = Context->GoodSeeds->GetOutPoint(SeedIndex);
			Settings->SeedMutations.ApplyToPoint(InCell.Get(), SeedPoint, PathIO->GetOut());
		}

		FPlatformAtomics::InterlockedIncrement(&OutputPathNum);
	}

	void FProcessor::CompleteWork()
	{
		if (!CellsConstraints->WrapperCell) { return; }
		if (OutputPathNum == 0 && WrapperSeed != -1 && Settings->Constraints.bKeepWrapperIfSolePath) { ProcessCell(WrapperSeed, CellsConstraints->WrapperCell); }
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExFindContoursContext, UPCGExFindContoursSettings>::Cleanup();
		if (CellsConstraints) { CellsConstraints->Cleanup(); }
	}

	void FBatch::Process()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FindContours)

		// Project positions
		ProjectionDetails = Settings->ProjectionDetails;
		if (!ProjectionDetails.Init(Context, VtxDataFacade)) { return; }
		PCGEx::InitArray(ProjectedPositions, VtxDataFacade->GetNum());

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ProjectionTaskGroup)

		ProjectionTaskGroup->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->OnProjectionComplete();
			};

		ProjectionTaskGroup->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				TArray<FVector>& PP = *This->ProjectedPositions;
				This->ProjectionDetails.ProjectFlat(This->VtxDataFacade, PP, Scope);
			};

		ProjectionTaskGroup->StartSubLoops(VtxDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}

	bool FBatch::PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor)
	{
		ClusterProcessor->ProjectedPositions = ProjectedPositions;
		TBatch<FProcessor>::PrepareSingle(ClusterProcessor);
		return true;
	}

	void FBatch::OnProjectionComplete()
	{
		TBatch<FProcessor>::Process();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
