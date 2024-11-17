// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingFindAllCells.h"

#include "PCGExCompare.h"


#define LOCTEXT_NAMESPACE "PCGExFindAllCells"
#define PCGEX_NAMESPACE FindAllCells

TArray<FPCGPinProperties> UPCGExFindAllCellsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputPathsLabel, "Contours", Required, {})
	//if (bOutputSeeds) { PCGEX_PIN_POINT(PCGExFindAllCells::OutputGoodSeedsLabel, "GoodSeeds", Required, {}) }
	return PinProperties;
}

PCGExData::EIOInit UPCGExFindAllCellsSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::NoOutput; }
PCGExData::EIOInit UPCGExFindAllCellsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(FindAllCells)

bool FPCGExFindAllCellsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindAllCells)

	PCGEX_FWD(ProjectionDetails)

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

		// Build constraint object
		CellsConstraints = MakeShared<PCGExTopology::FCellConstraints>(Settings->Constraints);
		CellsConstraints->DataBounds = Cluster->Bounds;

		StartParallelLoopForEdges(32); // Might be overkill low

		return true;
	}

	void FProcessor::ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FEdge& Edge, const int32 LoopIdx, const int32 Count)
	{
		//Check endpoints
		FVector G1 = FVector::ZeroVector;
		FVector G2 = FVector::ZeroVector;

		Cluster->GetProjectedEdgeGuides(Edge.Index, *ProjectedPositions, G1, G2);

		ProcessNodeCandidate(*Cluster->GetEdgeStart(Edge), Edge, G1);
		ProcessNodeCandidate(*Cluster->GetEdgeEnd(Edge), Edge, G2);
	}

	bool FProcessor::ProcessNodeCandidate(
		const PCGExCluster::FNode& Node,
		const PCGExGraph::FEdge& Edge,
		const FVector& Guide,
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

		const PCGExTopology::ECellResult Result = Cell->BuildFromCluster(Node.Index, Edge.Index, Guide, Cluster.ToSharedRef(), *ProjectedPositions);
		if (Result != PCGExTopology::ECellResult::Success) { return false; }

		TSharedRef<PCGExData::FPointIO> PathIO = Context->Paths->Emplace_GetRef<UPCGPointData>(VtxDataFacade->Source, PCGExData::EIOInit::NewOutput).ToSharedRef();
		PathIO->IOIndex = Edge.IOIndex; // Enforce seed order for collection output-ish

		PCGExGraph::CleanupClusterTags(PathIO, true);
		PCGExGraph::CleanupVtxData(PathIO);

		TSharedPtr<PCGExData::FFacade> PathDataFacade = MakeShared<PCGExData::FFacade>(PathIO);

		TArray<FPCGPoint> MutablePoints;
		MutablePoints.Reserve(Cell->Nodes.Num());

		const TArray<FPCGPoint>& InPoints = PathIO->GetIn()->GetPoints();
		for (int i = 0; i < Cell->Nodes.Num(); i++) { MutablePoints.Add(InPoints[Cluster->GetNode(Cell->Nodes[i])->PointIndex]); }
		Cell->PostProcessPoints(MutablePoints);

		PathIO->GetOut()->SetPoints(MutablePoints);

		if (!Cell->bIsClosedLoop) { if (Settings->bTagIfOpenPath) { PathIO->Tags->Add(Settings->IsOpenPathTag); } }
		else { if (Settings->bTagIfClosedLoop) { PathIO->Tags->Add(Settings->IsClosedLoopTag); } }

		if (Cell->bIsConvex) { if (Settings->bTagConvex) { PathIO->Tags->Add(Settings->ConvexTag); } }
		else { if (Settings->bTagConcave) { PathIO->Tags->Add(Settings->ConcaveTag); } }

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

		return true;
	}

	void FProcessor::EnsureRoamingClosedLoopProcessing()
	{
		if (NumAttempts == 0 && LastBinary != -1)
		{
			TSharedPtr<PCGExTopology::FCell> Cell = MakeShared<PCGExTopology::FCell>(CellsConstraints.ToSharedRef());

			FVector G1 = FVector::ZeroVector;
			FVector G2 = FVector::ZeroVector;

			PCGExGraph::FEdge& Edge = *Cluster->GetEdge(Cluster->GetNode(LastBinary)->Links[0].Edge);
			Cluster->GetProjectedEdgeGuides(Edge.Index, *ProjectedPositions, G1, G2);

			ProcessNodeCandidate(*Cluster->GetEdgeStart(Edge), Edge, G1, false);
		}
	}

	void FProcessor::OnEdgesProcessingComplete()
	{
		EnsureRoamingClosedLoopProcessing();
		TProcessor<FPCGExFindAllCellsContext, UPCGExFindAllCellsSettings>::OnEdgesProcessingComplete();
	}

	void FProcessor::CompleteWork()
	{
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
			[WeakThis = TWeakPtr<FBatch>(SharedThis(this))]()
			{
				if (TSharedPtr<FBatch> This = WeakThis.Pin()) { This->OnProjectionComplete(); }
			};

		ProjectionTaskGroup->OnSubLoopStartCallback =
			[WeakThis = TWeakPtr<FBatch>(SharedThis(this))]
			(const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				TSharedPtr<FBatch> This = WeakThis.Pin();
				if (!This) { return; }

				const int32 MaxIndex = StartIndex + Count;

				for (int i = StartIndex; i < MaxIndex; i++)
				{
					This->ProjectedPositions[i] = This->ProjectionDetails.ProjectFlat(This->VtxDataFacade->Source->GetInPoint(i).Transform.GetLocation(), i);
				}
			};

		ProjectionTaskGroup->StartSubLoops(VtxDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}

	bool FBatch::PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor)
	{
		ClusterProcessor->ProjectedPositions = &ProjectedPositions;
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
