// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingFindAllCells.h"

#include "PCGExCompare.h"


#define LOCTEXT_NAMESPACE "PCGExFindAllCells"
#define PCGEX_NAMESPACE FindAllCells

TArray<FPCGPinProperties> UPCGExFindAllCellsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGExTopology::SourceHolesLabel, "Omit cells that contain any points from this dataset", Normal, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExFindAllCellsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputPathsLabel, "Contours", Required, {})
	//if (bOutputSeeds) { PCGEX_PIN_POINT(PCGExFindAllCells::OutputGoodSeedsLabel, "GoodSeeds", Required, {}) }
	return PinProperties;
}

PCGExData::EIOInit UPCGExFindAllCellsSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::None; }
PCGExData::EIOInit UPCGExFindAllCellsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

PCGEX_INITIALIZE_ELEMENT(FindAllCells)

bool FPCGExFindAllCellsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindAllCells)

	PCGEX_FWD(Artifacts)
	if (!Context->Artifacts.Init(Context)) { return false; }

	if (TSharedPtr<PCGExData::FFacade> HoleDataFacade = PCGExData::TryGetSingleFacade(Context, PCGExTopology::SourceHolesLabel, false))
	{
		Context->Holes = MakeShared<PCGExTopology::FHoles>(Context, HoleDataFacade.ToSharedRef(), Settings->ProjectionDetails);
	}

	//const TSharedPtr<PCGExData::FPointIO> SeedsPoints = PCGExData::TryGetSingleInput(Context, PCGExGraph::SourceSeedsLabel, true);
	//if (!SeedsPoints) { return false; }

	Context->Paths = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->Paths->OutputPin = PCGExGraph::OutputPathsLabel;

	/*
	if (Settings->bOutputFilteredSeeds)
	{
		const int32 NumSeeds = SeedsPoints->GetNum();

		Context->SeedQuality.Init(false, NumSeeds);
		PCGEx::InitArray(Context->UdpatedSeedPoints, NumSeeds);

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

bool FPCGExFindAllCellsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindAllCellsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FindAllCells)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExFindAllCells::FBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExFindAllCells::FBatch>& NewBatch)
			{
				// NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	/*
	if (Settings->bOutputFilteredSeeds)
	{
		const TArray<FPCGPoint>& InSeeds = Context->SeedsDataFacade->Source->GetIn()->GetPoints();
		TArray<FPCGPoint>& GoodSeeds = Context->GoodSeeds->GetOut()->GetMutablePoints();
		TArray<FPCGPoint>& BadSeeds = Context->BadSeeds->GetOut()->GetMutablePoints();

		for (int i = 0; i < Context->SeedQuality.Num(); i++)
		{
			if (Context->SeedQuality[i]) { GoodSeeds.Add(Context->UdpatedSeedPoints[i]); }
			else { BadSeeds.Add(InSeeds[i]); }
		}

		Context->GoodSeeds->StageOutput();
		Context->BadSeeds->StageOutput();
	}
	*/

	Context->Paths->StageOutputs();

	return Context->TryComplete();
}


namespace PCGExFindAllCells
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFindAllCells::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		CellsConstraints = MakeShared<PCGExTopology::FCellConstraints>(Settings->Constraints);
		if (Settings->Constraints.bOmitWrappingBounds) { CellsConstraints->BuildWrapperCell(Cluster.ToSharedRef(), *ProjectedPositions); }
		CellsConstraints->Holes = Context->Holes;

		StartParallelLoopForEdges(32); // Might be overkill low

		return true;
	}

	void FProcessor::ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FEdge& Edge, const PCGExMT::FScope& Scope)
	{
		//Check endpoints
		FindCell(*Cluster->GetEdgeStart(Edge), Edge);
		FindCell(*Cluster->GetEdgeEnd(Edge), Edge);
	}

	bool FProcessor::FindCell(
		const PCGExCluster::FNode& Node,
		const PCGExGraph::FEdge& Edge,
		const bool bSkipBinary)
	{
		if (Node.IsBinary() && bSkipBinary)
		{
			FPlatformAtomics::InterlockedExchange(&LastBinary, Node.Index);
			return false;
		}

		if (!CellsConstraints->bKeepCellsWithLeaves && Node.IsLeaf()) { return false; }

		FPlatformAtomics::InterlockedAdd(&NumAttempts, 1);
		const TSharedPtr<PCGExTopology::FCell> Cell = MakeShared<PCGExTopology::FCell>(CellsConstraints.ToSharedRef());

		const PCGExTopology::ECellResult Result = Cell->BuildFromCluster(PCGExGraph::FLink(Node.Index, Edge.Index), Cluster.ToSharedRef(), *ProjectedPositions);
		if (Result != PCGExTopology::ECellResult::Success) { return false; }

		ProcessCell(Cell);

		return true;
	}

	void FProcessor::ProcessCell(const TSharedPtr<PCGExTopology::FCell>& InCell)
	{
		const TSharedPtr<PCGExData::FPointIO> PathIO = Context->Paths->Emplace_GetRef<UPCGPointData>(VtxDataFacade->Source, PCGExData::EIOInit::New);
		if (!PathIO) { return; }

		PathIO->Tags->Reset();                                          // Tag forwarding handled by artifacts
		PathIO->IOIndex = Cluster->GetEdge(InCell->Seed.Edge)->IOIndex; // Enforce seed order for collection output-ish

		PCGExGraph::CleanupClusterTags(PathIO);
		PCGExGraph::CleanupVtxData(PathIO);

		PCGEX_MAKE_SHARED(PathDataFacade, PCGExData::FFacade, PathIO.ToSharedRef())

		TArray<FPCGPoint> MutablePoints;
		MutablePoints.Reserve(InCell->Nodes.Num());

		const TArray<FPCGPoint>& InPoints = PathIO->GetIn()->GetPoints();
		for (int i = 0; i < InCell->Nodes.Num(); i++) { MutablePoints.Add(InPoints[Cluster->GetNode(InCell->Nodes[i])->PointIndex]); }
		InCell->PostProcessPoints(MutablePoints);

		PathIO->GetOut()->SetPoints(MutablePoints);

		Context->Artifacts.Process(Cluster, PathDataFacade, InCell);
		PathDataFacade->Write(AsyncManager);

		/*
		Context->SeedAttributesToPathTags.Tag(SeedIndex, PathIO);
		Context->SeedForwardHandler->Forward(SeedIndex, PathDataFacade);

		if (Settings->bFlagLeaves)
		{
			const TSharedPtr<PCGExData::TBuffer<bool>> LeavesBuffer = PathDataFacade->GetWritable(Settings->LeafAttributeName, false, true, PCGExData::EBufferInit::New);
			TArray<bool>& OutValues = *LeavesBuffer->GetOutValues();
			for (int i = 0; i < Cell->Nodes.Num(); i++) { OutValues[i] = Cluster->GetNode(Cell->Nodes[i])->Num() == 1; }
		}

		if (!Cell->bIsClosedLoop) { if (Settings->bTagIfOpenPath) { PathIO->Tags->Add(Settings->IsOpenPathTag); } }
		else { if (Settings->bTagIfClosedLoop) { PathIO->Tags->Add(Settings->IsClosedLoopTag); } }

		PathDataFacade->Write(AsyncManager);

		if (Settings->bOutputFilteredSeeds)
		{
			Context->SeedQuality[SeedIndex] = true;
			FPCGPoint SeedPoint = Context->SeedsDataFacade->Source->GetInPoint(SeedIndex);
			Settings->SeedMutations.ApplyToPoint(Cell.Get(), SeedPoint, MutablePoints);
			Context->UdpatedSeedPoints[SeedIndex] = SeedPoint;
		}
		*/

		FPlatformAtomics::InterlockedIncrement(&OutputPathsNum);
	}

	void FProcessor::EnsureRoamingClosedLoopProcessing()
	{
		if (NumAttempts == 0 && LastBinary != -1)
		{
			PCGEX_MAKE_SHARED(Cell, PCGExTopology::FCell, CellsConstraints.ToSharedRef())
			PCGExGraph::FEdge& Edge = *Cluster->GetEdge(Cluster->GetNode(LastBinary)->Links[0].Edge);
			FindCell(*Cluster->GetEdgeStart(Edge), Edge, false);
		}
	}

	void FProcessor::OnEdgesProcessingComplete()
	{
		EnsureRoamingClosedLoopProcessing();
		TProcessor<FPCGExFindAllCellsContext, UPCGExFindAllCellsSettings>::OnEdgesProcessingComplete();
	}

	void FProcessor::CompleteWork()
	{
		if (!CellsConstraints->WrapperCell) { return; }
		if (OutputPathsNum == 0 && Settings->Constraints.bKeepWrapperIfSolePath) { ProcessCell(CellsConstraints->WrapperCell); }
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExFindAllCellsContext, UPCGExFindAllCellsSettings>::Cleanup();
		CellsConstraints->Cleanup();
	}

	void FBatch::Process()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FindAllCells)

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
