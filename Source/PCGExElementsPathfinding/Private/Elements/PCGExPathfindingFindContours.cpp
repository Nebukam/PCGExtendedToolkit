// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExPathfindingFindContours.h"


#include "Data/PCGExData.h"
#include "Data/PCGPointArrayData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Clusters/PCGExCluster.h"
#include "Clusters/PCGExClustersHelpers.h"
#include "Clusters/Artifacts/PCGExCell.h"
#include "Data/Utils/PCGExDataForward.h"
#include "Paths/PCGExPath.h"
#include "Paths/PCGExPathsCommon.h"

#define LOCTEXT_NAMESPACE "PCGExFindContours"
#define PCGEX_NAMESPACE FindContours

TArray<FPCGPinProperties> UPCGExFindContoursSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGExCommon::Labels::SourceSeedsLabel, "Seeds associated with the main input points", Required)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExFindContoursSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExPaths::Labels::OutputPathsLabel, "Contours", Required)
	if (bOutputFilteredSeeds)
	{
		PCGEX_PIN_POINT(PCGExFindContours::OutputGoodSeedsLabel, "GoodSeeds", Required)
		PCGEX_PIN_POINT(PCGExFindContours::OutputBadSeedsLabel, "BadSeeds", Required)
	}
	return PinProperties;
}

PCGExData::EIOInit UPCGExFindContoursSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::NoInit; }
PCGExData::EIOInit UPCGExFindContoursSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NoInit; }

PCGEX_INITIALIZE_ELEMENT(FindContours)
PCGEX_ELEMENT_BATCH_EDGE_IMPL(FindContours)

bool FPCGExFindContoursElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindContours)

	PCGEX_FWD(Artifacts)
	if (!Context->Artifacts.Init(Context)) { return false; }

	Context->SeedsDataFacade = PCGExData::TryGetSingleFacade(Context, PCGExCommon::Labels::SourceSeedsLabel, false, true);
	if (!Context->SeedsDataFacade) { return false; }

	PCGEX_FWD(SeedAttributesToPathTags)
	if (!Context->SeedAttributesToPathTags.Init(Context, Context->SeedsDataFacade)) { return false; }
	Context->SeedForwardHandler = Settings->SeedForwarding.GetHandler(Context->SeedsDataFacade);

	Context->OutputPaths = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->OutputPaths->OutputPin = PCGExPaths::Labels::OutputPathsLabel;

	if (Settings->bOutputFilteredSeeds)
	{
		const int32 NumSeeds = Context->SeedsDataFacade->GetNum();

		Context->SeedQuality.Init(false, NumSeeds);

		Context->GoodSeeds = NewPointIO(Context->SeedsDataFacade->Source, PCGExFindContours::OutputGoodSeedsLabel);
		Context->GoodSeeds->InitializeOutput(PCGExData::EIOInit::Duplicate);
		PCGExPointArrayDataHelpers::SetNumPointsAllocated(Context->GoodSeeds->GetOut(), NumSeeds);

		Context->BadSeeds = NewPointIO(Context->SeedsDataFacade->Source, PCGExFindContours::OutputBadSeedsLabel);
		Context->BadSeeds->InitializeOutput(PCGExData::EIOInit::Duplicate);
		PCGExPointArrayDataHelpers::SetNumPointsAllocated(Context->BadSeeds->GetOut(), NumSeeds);
	}

	return true;
}

bool FPCGExFindContoursElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindContoursElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FindContours)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters([](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; }, [&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
		{
			//NewBatch->bRequiresWriteStep = Settings->Artifacts.WriteAny();
			NewBatch->bSkipCompletion = true;
			NewBatch->SetProjectionDetails(Settings->ProjectionDetails);
		}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	if (Settings->bOutputFilteredSeeds)
	{
		(void)Context->GoodSeeds->Gather(Context->SeedQuality);
		(void)Context->BadSeeds->Gather(Context->SeedQuality, true);

		(void)Context->GoodSeeds->StageOutput(Context);
		(void)Context->BadSeeds->StageOutput(Context);
	}

	Context->OutputPaths->StageOutputs();

	return Context->TryComplete();
}


namespace PCGExFindContours
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFindContours::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

		if (Settings->bUseOctreeSearch) { Cluster->RebuildOctree(Settings->SeedPicking.PickingMethod); }
		Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Edge); // We need edge octree anyway

		const int32 NumSeeds = Context->SeedsDataFacade->Source->GetNum();

		CellsConstraints = MakeShared<PCGExClusters::FCellConstraints>(Settings->Constraints);
		CellsConstraints->Reserve(NumSeeds);
		if (Settings->Constraints.bOmitWrappingBounds) { CellsConstraints->BuildWrapperCell(Cluster.ToSharedRef(), *ProjectedVtxPositions.Get()); }
		if (CellsConstraints->WrapperCell) { CellsConstraints->WrapperCell->CustomIndex = WrapperSeed; }

		StartParallelLoopForRange(NumSeeds, 64);

		return true;
	}

	void FProcessor::PrepareLoopScopesForRanges(const TArray<PCGExMT::FScope>& Loops)
	{
		ScopedValidCells = MakeShared<PCGExMT::TScopedArray<TSharedPtr<PCGExClusters::FCell>>>(Loops);
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		TConstPCGValueRange<FTransform> InSeedTransforms = Context->SeedsDataFacade->GetIn()->GetConstTransformValueRange();

		const TArray<FVector2D>& ProjectedPositions = *ProjectedVtxPositions.Get();

		TArray<TSharedPtr<PCGExClusters::FCell>>& CellsContainer = ScopedValidCells->Get_Ref(Scope);
		CellsContainer.Reserve(Scope.Count * 2);

		PCGEX_SCOPE_LOOP(Index)
		{
			const FVector SeedWP = InSeedTransforms[Index].GetLocation();

			const TSharedPtr<PCGExClusters::FCell> Cell = MakeShared<PCGExClusters::FCell>(CellsConstraints.ToSharedRef());
			const PCGExClusters::ECellResult Result = Cell->BuildFromCluster(SeedWP, Cluster.ToSharedRef(), ProjectedPositions, ProjectionDetails.ProjectionNormal, &Settings->SeedPicking);

			if (Result != PCGExClusters::ECellResult::Success)
			{
				if (Result == PCGExClusters::ECellResult::WrapperCell || (CellsConstraints->WrapperCell && CellsConstraints->WrapperCell->GetCellHash() == Cell->GetCellHash()))
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

			Cell->CustomIndex = Index;
			CellsContainer.Add(Cell);
		}
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		ScopedValidCells->Collapse(ValidCells);
		const int32 NumCells = ValidCells.Num();

		if (!NumCells)
		{
			bIsProcessorValid = false;
			return;
		}

		CellsIOIndices.Reserve(NumCells);

		Context->OutputPaths->IncreaseReserve(NumCells + 1);
		for (int i = 0; i < NumCells; i++)
		{
			CellsIOIndices.Add(Context->OutputPaths->Emplace_GetRef(VtxDataFacade->Source, PCGExData::EIOInit::New));
		}

		if (CellsConstraints->WrapperCell && ValidCells.IsEmpty() && WrapperSeed != -1 && Settings->Constraints.bKeepWrapperIfSolePath)
		{
			// Process wrapper cell if it's the only valid cell and it's not omitted
			ProcessCell(CellsConstraints->WrapperCell, Context->OutputPaths->Emplace_GetRef(VtxDataFacade->Source, PCGExData::EIOInit::New));
			return;
		}

		PCGEX_ASYNC_GROUP_CHKD_VOID(TaskManager, ProcessCellsTask)

		ProcessCellsTask->OnSubLoopStartCallback = [PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
		{
			PCGEX_ASYNC_THIS
			TArray<TSharedPtr<PCGExClusters::FCell>>& ValidCells_Ref = This->ValidCells;
			const TArray<TSharedPtr<PCGExData::FPointIO>>& CellsIOIndices_Ref = This->CellsIOIndices;

			PCGEX_SCOPE_LOOP(Index)
			{
				if (const TSharedPtr<PCGExData::FPointIO> IO = CellsIOIndices_Ref[Index]) { This->ProcessCell(ValidCells_Ref[Index], IO); }
				ValidCells_Ref[Index] = nullptr;
			}
		};

		ProcessCellsTask->StartSubLoops(CellsIOIndices.Num(), 64);
	}

	void FProcessor::ProcessCell(const TSharedPtr<PCGExClusters::FCell>& InCell, const TSharedPtr<PCGExData::FPointIO>& PathIO)
	{
		if (!PathIO) { return; }

		const int32 SeedIndex = InCell->CustomIndex;
		const int32 NumCellPoints = InCell->Nodes.Num();
		PCGExPointArrayDataHelpers::SetNumPointsAllocated(PathIO->GetOut(), NumCellPoints);

		PathIO->Tags->Reset();                              // Tag forwarding handled by artifacts
		PathIO->IOIndex = BatchIndex * 1000000 + SeedIndex; // Enforce seed order for collection output

		PCGExClusters::Helpers::CleanupClusterData(PathIO);

		PCGEX_MAKE_SHARED(PathDataFacade, PCGExData::FFacade, PathIO.ToSharedRef())

		TArray<int32> ReadIndices;
		ReadIndices.SetNumUninitialized(NumCellPoints);

		for (int i = 0; i < NumCellPoints; i++) { ReadIndices[i] = Cluster->GetNodePointIndex(InCell->Nodes[i]); }

		PathIO->InheritPoints(ReadIndices, 0);
		InCell->PostProcessPoints(PathIO->GetOut());

		Context->SeedAttributesToPathTags.Tag(Context->SeedsDataFacade->GetInPoint(SeedIndex), PathIO);
		Context->SeedForwardHandler->Forward(SeedIndex, PathDataFacade);

		Context->Artifacts.Process(Cluster, PathDataFacade, InCell);
		PathDataFacade->WriteFastest(TaskManager);

		if (Settings->bOutputFilteredSeeds)
		{
			Context->SeedQuality[SeedIndex] = true;
			PCGExData::FMutablePoint SeedPoint = Context->GoodSeeds->GetOutPoint(SeedIndex);
			Settings->SeedMutations.ApplyToPoint(InCell.Get(), SeedPoint, PathIO->GetOut());
		}
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExFindContoursContext, UPCGExFindContoursSettings>::Cleanup();
		if (CellsConstraints) { CellsConstraints->Cleanup(); }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
