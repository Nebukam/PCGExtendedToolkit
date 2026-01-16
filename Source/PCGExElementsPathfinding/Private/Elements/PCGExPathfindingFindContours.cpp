// Copyright 2026 Timothé Lapetite and contributors
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
#include "Math/Geo/PCGExGeo.h"
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

		const int32 NumSeeds = Context->SeedsDataFacade->Source->GetNum();

		// Set up cell constraints
		CellsConstraints = MakeShared<PCGExClusters::FCellConstraints>(Settings->Constraints);
		CellsConstraints->Reserve(Cluster->Edges->Num());

		// Use FPlanarFaceEnumerator (DCEL-based) to find ALL cells first
		PCGExClusters::FPlanarFaceEnumerator Enumerator;
		Enumerator.Build(Cluster.ToSharedRef(), *ProjectedVtxPositions.Get());

		// Enumerate all cells
		TArray<TSharedPtr<PCGExClusters::FCell>> AllCells;
		Enumerator.EnumerateAllFaces(AllCells, CellsConstraints.ToSharedRef());

		// If we should omit wrapping bounds, find and store the wrapper cell
		if (Settings->Constraints.bOmitWrappingBounds && !AllCells.IsEmpty())
		{
			double MaxArea = -MAX_dbl;
			int32 WrapperIdx = INDEX_NONE;
			for (int32 i = 0; i < AllCells.Num(); ++i)
			{
				if (AllCells[i] && AllCells[i]->Data.Area > MaxArea)
				{
					MaxArea = AllCells[i]->Data.Area;
					WrapperIdx = i;
				}
			}
			if (WrapperIdx != INDEX_NONE)
			{
				CellsConstraints->WrapperCell = AllCells[WrapperIdx];
				AllCells.RemoveAt(WrapperIdx);
			}
		}

		if (AllCells.IsEmpty())
		{
			// Check if we should output the wrapper cell as the sole cell
			if (CellsConstraints->WrapperCell && Settings->Constraints.bKeepWrapperIfSolePath)
			{
				// Find seed closest to bounds center for wrapper
				TConstPCGValueRange<FTransform> InSeedTransforms = Context->SeedsDataFacade->GetIn()->GetConstTransformValueRange();
				double ClosestDist = MAX_dbl;
				int32 ClosestSeed = -1;
				for (int32 i = 0; i < NumSeeds; ++i)
				{
					const double Dist = FVector::DistSquared(InSeedTransforms[i].GetLocation(), Cluster->Bounds.GetCenter());
					if (Dist < ClosestDist)
					{
						ClosestDist = Dist;
						ClosestSeed = i;
					}
				}
				if (ClosestSeed != -1)
				{
					CellsConstraints->WrapperCell->CustomIndex = ClosestSeed;
					ProcessCell(CellsConstraints->WrapperCell, Context->OutputPaths->Emplace_GetRef(VtxDataFacade->Source, PCGExData::EIOInit::New));
				}
			}
			return true;
		}

		// Project all seeds to 2D and store them
		ProjectedSeeds.SetNumUninitialized(NumSeeds);
		TConstPCGValueRange<FTransform> InSeedTransforms = Context->SeedsDataFacade->GetIn()->GetConstTransformValueRange();
		for (int32 i = 0; i < NumSeeds; ++i)
		{
			const FVector ProjectedSeed3D = ProjectionDetails.ProjectFlat(InSeedTransforms[i].GetLocation());
			ProjectedSeeds[i] = FVector2D(ProjectedSeed3D.X, ProjectedSeed3D.Y);
		}

		// Store all cells for parallel processing - we'll filter by seed containment
		EnumeratedCells = MoveTemp(AllCells);

		// Process cells in parallel to find which seeds they contain
		StartParallelLoopForRange(EnumeratedCells.Num(), 64);

		return true;
	}

	void FProcessor::PrepareLoopScopesForRanges(const TArray<PCGExMT::FScope>& Loops)
	{
		ScopedValidCells = MakeShared<PCGExMT::TScopedArray<TSharedPtr<PCGExClusters::FCell>>>(Loops);
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		const int32 NumSeeds = ProjectedSeeds.Num();

		TArray<TSharedPtr<PCGExClusters::FCell>>& CellsContainer = ScopedValidCells->Get_Ref(Scope);
		CellsContainer.Reserve(Scope.Count);

		PCGEX_SCOPE_LOOP(CellIndex)
		{
			const TSharedPtr<PCGExClusters::FCell>& Cell = EnumeratedCells[CellIndex];
			if (!Cell || Cell->Polygon.IsEmpty()) { continue; }

			// Find the first seed that is inside this cell
			int32 ContainingSeedIndex = -1;
			for (int32 SeedIdx = 0; SeedIdx < NumSeeds; ++SeedIdx)
			{
				if (PCGExMath::Geo::IsPointInPolygon(ProjectedSeeds[SeedIdx], Cell->Polygon))
				{
					ContainingSeedIndex = SeedIdx;
					break;
				}
			}

			// Only output cells that contain at least one seed
			if (ContainingSeedIndex != -1)
			{
				Cell->CustomIndex = ContainingSeedIndex;
				CellsContainer.Add(Cell);
			}
		}
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		ScopedValidCells->Collapse(ValidCells);
		const int32 NumCells = ValidCells.Num();

		if (NumCells == 0)
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
