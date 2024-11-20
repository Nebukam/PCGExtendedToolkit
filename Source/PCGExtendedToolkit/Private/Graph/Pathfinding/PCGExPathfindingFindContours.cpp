// Copyright Timothé Lapetite 2024
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
	PCGEX_PIN_POINTS(PCGExGraph::OutputPathsLabel, "Contours", Required, {})
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

	if (Settings->bFlagLeaves) { PCGEX_VALIDATE_NAME(Settings->LeafAttributeName); }

	const TSharedPtr<PCGExData::FPointIO> SeedsPoints = PCGExData::TryGetSingleInput(Context, PCGExGraph::SourceSeedsLabel, true);
	if (!SeedsPoints) { return false; }

	Context->SeedsDataFacade = MakeShared<PCGExData::FFacade>(SeedsPoints.ToSharedRef());

	if (!Context->ProjectionDetails.Init(Context, Context->SeedsDataFacade)) { return false; }

	PCGEX_FWD(SeedAttributesToPathTags)
	if (!Context->SeedAttributesToPathTags.Init(Context, Context->SeedsDataFacade)) { return false; }
	Context->SeedForwardHandler = Settings->SeedForwarding.GetHandler(Context->SeedsDataFacade);

	Context->Paths = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->Paths->OutputPin = PCGExGraph::OutputPathsLabel;

	if (Settings->bOutputFilteredSeeds)
	{
		const int32 NumSeeds = SeedsPoints->GetNum();

		Context->SeedQuality.Init(false, NumSeeds);
		PCGEx::InitArray(Context->UdpatedSeedPoints, NumSeeds);

		Context->GoodSeeds = NewPointIO(SeedsPoints.ToSharedRef(), PCGExFindContours::OutputGoodSeedsLabel);
		Context->GoodSeeds->InitializeOutput(PCGExData::EIOInit::New);
		Context->GoodSeeds->GetOut()->GetMutablePoints().Reserve(NumSeeds);

		Context->BadSeeds = NewPointIO(SeedsPoints.ToSharedRef(), PCGExFindContours::OutputBadSeedsLabel);
		Context->BadSeeds->InitializeOutput(PCGExData::EIOInit::New);
		Context->BadSeeds->GetOut()->GetMutablePoints().Reserve(NumSeeds);
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
				if (Settings->bFlagLeaves)
				{
					NewBatch->bRequiresWriteStep = true;
				}
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

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

		// Build constraint object
		CellsConstraints = MakeShared<PCGExTopology::FCellConstraints>(Settings->Constraints);
		CellsConstraints->DataBounds = Cluster->Bounds;

		if (Settings->bUseOctreeSearch) { Cluster->RebuildOctree(Settings->SeedPicking.PickingMethod); }
		Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Edge); // We need edge octree anyway

		StartParallelLoopForRange(Context->SeedsDataFacade->Source->GetNum(), 64);

		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(int32 Iteration, const int32 LoopIdx, const int32 Count)
	{
		TryFindContours(Iteration);
	}

	void FProcessor::TryFindContours(const int32 SeedIndex) const
	{
		const FVector ProjectedSeedPosition = Context->ProjectionDetails.Project(Context->SeedsDataFacade->Source->GetInPoint(SeedIndex).Transform.GetLocation(), SeedIndex);

		const FVector RealSeedPosition = Context->SeedsDataFacade->Source->GetInPoint(SeedIndex).Transform.GetLocation();
		const int32 StartNodeIndex = Cluster->FindClosestNode(RealSeedPosition, Settings->SeedPicking.PickingMethod, 2);

		if (StartNodeIndex == -1) { return; } // Fail. Either single-node or single-edge cluster, or no connected edge

		const int32 NextEdge = Cluster->FindClosestEdge(StartNodeIndex, RealSeedPosition);

		if (NextEdge == -1) { return; } // Fail. Either single-node or single-edge cluster, or no connected edge

		const FVector StartPosition = Cluster->GetPos(StartNodeIndex);
		if (!Settings->SeedPicking.WithinDistance(StartPosition, RealSeedPosition)) { return; } // Fail. Not within radius.

		const TSharedPtr<PCGExTopology::FCell> Cell = MakeShared<PCGExTopology::FCell>(CellsConstraints.ToSharedRef());

		const PCGExTopology::ECellResult Result = Cell->BuildFromCluster(StartNodeIndex, NextEdge, ProjectedSeedPosition, Cluster.ToSharedRef(), *ProjectedPositions);
		if (Result != PCGExTopology::ECellResult::Success) { return; }

		TSharedRef<PCGExData::FPointIO> PathIO = Context->Paths->Emplace_GetRef<UPCGPointData>(VtxDataFacade->Source, PCGExData::EIOInit::New).ToSharedRef();
		PathIO->IOIndex = SeedIndex; // Enforce seed order for collection output

		PCGExGraph::CleanupClusterTags(PathIO, true);
		PCGExGraph::CleanupVtxData(PathIO);

		TSharedPtr<PCGExData::FFacade> PathDataFacade = MakeShared<PCGExData::FFacade>(PathIO);

		TArray<FPCGPoint> MutablePoints;
		MutablePoints.Reserve(Cell->Nodes.Num());

		const TArray<FPCGPoint>& InPoints = PathIO->GetIn()->GetPoints();
		for (int i = 0; i < Cell->Nodes.Num(); i++) { MutablePoints.Add(InPoints[Cluster->GetNode(Cell->Nodes[i])->PointIndex]); }
		Cell->PostProcessPoints(MutablePoints);

		PathIO->GetOut()->SetPoints(MutablePoints);

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

		if (Cell->bIsConvex) { if (Settings->bTagConvex) { PathIO->Tags->Add(Settings->ConvexTag); } }
		else { if (Settings->bTagConcave) { PathIO->Tags->Add(Settings->ConcaveTag); } }

		PathDataFacade->Write(AsyncManager);

		if (Settings->bOutputFilteredSeeds)
		{
			Context->SeedQuality[SeedIndex] = true;
			FPCGPoint SeedPoint = Context->SeedsDataFacade->Source->GetInPoint(SeedIndex);
			Settings->SeedMutations.ApplyToPoint(Cell.Get(), SeedPoint, MutablePoints);
			Context->UdpatedSeedPoints[SeedIndex] = SeedPoint;
		}
	}

	void FProcessor::CompleteWork()
	{
		TProcessor<FPCGExFindContoursContext, UPCGExFindContoursSettings>::CompleteWork();
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
