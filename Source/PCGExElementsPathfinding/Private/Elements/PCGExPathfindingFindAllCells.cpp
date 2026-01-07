// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExPathfindingFindAllCells.h"


#include "Data/PCGExData.h"
#include "Data/PCGPointArrayData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Clusters/PCGExCluster.h"
#include "Clusters/PCGExClustersHelpers.h"
#include "Clusters/Artifacts/PCGExCell.h"
#include "Paths/PCGExPath.h"
#include "Paths/PCGExPathsCommon.h"

#define LOCTEXT_NAMESPACE "PCGExFindAllCells"
#define PCGEX_NAMESPACE FindAllCells

TArray<FPCGPinProperties> UPCGExFindAllCellsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGExClusters::Labels::SourceHolesLabel, "Omit cells that contain any points from this dataset", Normal)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExFindAllCellsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExPaths::Labels::OutputPathsLabel, "Contours", Required)
	//if (bOutputSeeds) { PCGEX_PIN_POINT(PCGExFindAllCells::OutputGoodSeedsLabel, "GoodSeeds", Required, {}) }
	return PinProperties;
}

PCGExData::EIOInit UPCGExFindAllCellsSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::NoInit; }
PCGExData::EIOInit UPCGExFindAllCellsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NoInit; }

PCGEX_INITIALIZE_ELEMENT(FindAllCells)
PCGEX_ELEMENT_BATCH_EDGE_IMPL(FindAllCells)

bool FPCGExFindAllCellsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindAllCells)

	PCGEX_FWD(Artifacts)
	if (!Context->Artifacts.Init(Context)) { return false; }

	Context->HolesFacade = PCGExData::TryGetSingleFacade(Context, PCGExClusters::Labels::SourceHolesLabel, false, false);
	if (Context->HolesFacade && Settings->ProjectionDetails.Method == EPCGExProjectionMethod::Normal)
	{
		Context->Holes = MakeShared<PCGExClusters::FHoles>(Context, Context->HolesFacade.ToSharedRef(), Settings->ProjectionDetails);
	}

	//const TSharedPtr<PCGExData::FPointIO> SeedsPoints = PCGExData::TryGetSingleInput(Context, PCGExCommon::Labels::SourceSeedsLabel, true);
	//if (!SeedsPoints) { return false; }

	Context->OutputPaths = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->OutputPaths->OutputPin = PCGExPaths::Labels::OutputPathsLabel;

	/*
	if (Settings->bOutputFilteredSeeds)
	{
		const int32 NumSeeds = SeedsPoints->GetNum();

		Context->SeedQuality.Init(false, NumSeeds);
		PCGExArrayHelpers::InitArray(Context->UdpatedSeedPoints, NumSeeds);

		Context->GoodSeeds = NewPointIO(SeedsPoints.ToSharedRef(), PCGExFindAllCells::OutputGoodSeedsLabel);
		Context->GoodSeeds->InitializeOutput(PCGExData::EIOInit::NewOutput);
		Context->GoodSeeds->GetOut()->GetMutablePoints().Reserve(NumSeeds);

		Context->BadSeeds = NewPointIO(SeedsPoints.ToSharedRef(), PCGExFindAllCells::OutputBadSeedsLabel);
		Context->BadSeeds->InitializeOutput(PCGExData::EIOInit::NewOutput);
		Context->BadSeeds->GetOut()->GetMutablePoints().Reserve(NumSeeds);
	}
	*/

	return true;
}

bool FPCGExFindAllCellsElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindAllCellsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FindAllCells)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters([](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; }, [&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
		{
			NewBatch->bSkipCompletion = true;
			NewBatch->SetProjectionDetails(Settings->ProjectionDetails);
		}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	// TODO : Output seeds?

	Context->OutputPaths->StageOutputs();

	return Context->TryComplete();
}


namespace PCGExFindAllCells
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFindAllCells::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

		if (Context->HolesFacade)
		{
			Holes = Context->Holes ? Context->Holes : MakeShared<PCGExClusters::FHoles>(Context, Context->HolesFacade.ToSharedRef(), ProjectionDetails);
		}

		CellsConstraints = MakeShared<PCGExClusters::FCellConstraints>(Settings->Constraints);
		CellsConstraints->Reserve(Cluster->Edges->Num());
		if (Settings->Constraints.bOmitWrappingBounds) { CellsConstraints->BuildWrapperCell(Cluster.ToSharedRef(), *ProjectedVtxPositions.Get()); }
		CellsConstraints->Holes = Holes;

		StartParallelLoopForEdges(32); // Might be overkill low

		return true;
	}

	void FProcessor::PrepareLoopScopesForEdges(const TArray<PCGExMT::FScope>& Loops)
	{
		ScopedValidCells = MakeShared<PCGExMT::TScopedArray<TSharedPtr<PCGExClusters::FCell>>>(Loops);
	}

	void FProcessor::ProcessEdges(const PCGExMT::FScope& Scope)
	{
		TArray<PCGExGraphs::FEdge>& ClusterEdges = *Cluster->Edges;
		TArray<TSharedPtr<PCGExClusters::FCell>>& CellsContainer = ScopedValidCells->Get_Ref(Scope);
		CellsContainer.Reserve(Scope.Count * 2);

		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFindAllCells::ProcessEdges);

		PCGEX_SCOPE_LOOP(Index)
		{
			PCGExGraphs::FEdge& Edge = ClusterEdges[Index];

			//Check endpoints
			FindCell(*Cluster->GetEdgeStart(Edge), Edge, CellsContainer);
			FindCell(*Cluster->GetEdgeEnd(Edge), Edge, CellsContainer);
		}

		CellsContainer.Shrink();
	}

	bool FProcessor::FindCell(const PCGExClusters::FNode& Node, const PCGExGraphs::FEdge& Edge, TArray<TSharedPtr<PCGExClusters::FCell>>& Scope, const bool bSkipBinary)
	{
		if (Node.IsBinary() && bSkipBinary)
		{
			FPlatformAtomics::InterlockedExchange(&LastBinary, Node.Index);
			return false;
		}

		if (!CellsConstraints->bKeepCellsWithLeaves && Node.IsLeaf()) { return false; }

		FPlatformAtomics::InterlockedAdd(&NumAttempts, 1);
		const TSharedPtr<PCGExClusters::FCell> Cell = MakeShared<PCGExClusters::FCell>(CellsConstraints.ToSharedRef());

		const PCGExClusters::ECellResult Result = Cell->BuildFromCluster(PCGExGraphs::FLink(Node.Index, Edge.Index), Cluster.ToSharedRef(), *ProjectedVtxPositions.Get());
		if (Result != PCGExClusters::ECellResult::Success) { return false; }

		Scope.Add(Cell);

		return true;
	}

	void FProcessor::ProcessCell(const TSharedPtr<PCGExClusters::FCell>& InCell, const TSharedPtr<PCGExData::FPointIO>& PathIO)
	{
		if (!PathIO) { return; }

		// Tag forwarding handled by artifacts
		PathIO->Tags->Reset();

		// Enforce IO index based on edge IO + smallest node point index, for deterministic output order
		PathIO->IOIndex = EdgeDataFacade->Source->IOIndex * 1000000 + Cluster->GetNodePointIndex(InCell->Nodes[0]);

		PCGExClusters::Helpers::CleanupClusterData(PathIO);

		PCGEX_MAKE_SHARED(PathDataFacade, PCGExData::FFacade, PathIO.ToSharedRef())

		TArray<int32> ReadIndices;
		ReadIndices.SetNumUninitialized(InCell->Nodes.Num());

		for (int i = 0; i < InCell->Nodes.Num(); i++) { ReadIndices[i] = Cluster->GetNodePointIndex(InCell->Nodes[i]); }
		PathIO->InheritPoints(ReadIndices, 0);
		InCell->PostProcessPoints(PathIO->GetOut());

		Context->Artifacts.Process(Cluster, PathDataFacade, InCell);
		PathDataFacade->WriteFastest(TaskManager);

		// TODO : Create cell centroids here
	}

	void FProcessor::EnsureRoamingClosedLoopProcessing()
	{
		if (NumAttempts == 0 && LastBinary != -1)
		{
			PCGEX_MAKE_SHARED(Cell, PCGExClusters::FCell, CellsConstraints.ToSharedRef())
			PCGExGraphs::FEdge& Edge = *Cluster->GetEdge(Cluster->GetNode(LastBinary)->Links[0].Edge);
			FindCell(*Cluster->GetEdgeStart(Edge), Edge, *ScopedValidCells->Arrays[0].Get(), false);
		}
	}

	void FProcessor::OnEdgesProcessingComplete()
	{
		EnsureRoamingClosedLoopProcessing();
		ScopedValidCells->Collapse(ValidCells);

		const int32 NumCells = ValidCells.Num();
		CellsIO.Reserve(NumCells);

		Context->OutputPaths->IncreaseReserve(NumCells + 1);
		for (int i = 0; i < NumCells; i++)
		{
			CellsIO.Add(Context->OutputPaths->Emplace_GetRef(VtxDataFacade->Source, PCGExData::EIOInit::New));
		}

		if (CellsConstraints->WrapperCell && ValidCells.IsEmpty() && Settings->Constraints.bKeepWrapperIfSolePath)
		{
			// Process wrapper cell, it's the only valid one and we want it.
			ProcessCell(CellsConstraints->WrapperCell, Context->OutputPaths->Emplace_GetRef(VtxDataFacade->Source, PCGExData::EIOInit::New));
			return;
		}

		if (ValidCells.IsEmpty()) { return; }

		StartParallelLoopForRange(ValidCells.Num());
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			if (const TSharedPtr<PCGExData::FPointIO> IO = CellsIO[Index]) { ProcessCell(ValidCells[Index], IO); }
			ValidCells[Index] = nullptr;
		}
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExFindAllCellsContext, UPCGExFindAllCellsSettings>::Cleanup();
		CellsConstraints->Cleanup();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
