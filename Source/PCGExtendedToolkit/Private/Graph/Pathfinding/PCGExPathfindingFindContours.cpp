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

PCGExData::EIOInit UPCGExFindContoursSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::NoOutput; }
PCGExData::EIOInit UPCGExFindContoursSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NoOutput; }

bool FPCGExFindContoursContext::TryFindContours(
	const TSharedPtr<PCGExData::FPointIO>& PathIO,
	const int32 SeedIndex,
	const FVector& ProjectedSeedPosition,
	TSharedPtr<PCGExFindContours::FProcessor> ClusterProcessor)
{
	const UPCGExFindContoursSettings* Settings = ClusterProcessor->GetSettings();
	TSharedPtr<PCGExCluster::FCluster> Cluster = ClusterProcessor->Cluster;
	TSharedPtr<TArray<PCGExCluster::FExpandedNode>> ExpandedNodes = ClusterProcessor->ExpandedNodes;

	const FVector RealSeedPosition = ClusterProcessor->GetContext()->SeedsDataFacade->Source->GetInPoint(SeedIndex).Transform.GetLocation();
	const int32 StartNodeIndex = Cluster->FindClosestNode(RealSeedPosition, Settings->SeedPicking.PickingMethod, 2);

	if (StartNodeIndex == -1) { return false; } // Fail. Either single-node or single-edge cluster, or no connected edge

	const int32 NextEdge = Cluster->FindClosestEdge(StartNodeIndex, RealSeedPosition);

	if (NextEdge == -1) { return false; } // Fail. Either single-node or single-edge cluster, or no connected edge

	const FVector StartPosition = Cluster->GetPos(StartNodeIndex);
	if (!Settings->SeedPicking.WithinDistance(StartPosition, RealSeedPosition))
	{
		// Fail. Not within radius.
		return false;
	}

	TSharedPtr<PCGExTopology::FCell> Cell = MakeShared<PCGExTopology::FCell>(ClusterProcessor->TopologyConstraints.ToSharedRef());

	const PCGExTopology::ECellResult Result = Cell->BuildFromCluster(StartNodeIndex, NextEdge, ProjectedSeedPosition, Cluster.ToSharedRef(), *ClusterProcessor->ProjectedPositions, ClusterProcessor->ExpandedNodes);

	if (Result != PCGExTopology::ECellResult::Success) { return false; }

	if (Settings->OmitPathsByBounds == EPCGExOmitPathsByBounds::NearlyEqualClusterBounds)
	{
		if (PCGExCompare::NearlyEqual(Cell->Bounds.GetSize().Length(), ClusterProcessor->Cluster->Bounds.GetSize().Length(), Settings->BoundsSizeTolerance))
		{
			return false;
		}
	}

	PathIO->InitializeOutput(PCGExData::EIOInit::NewOutput);
	PCGExGraph::CleanupClusterTags(PathIO, true);
	PCGExGraph::CleanupVtxData(PathIO);

	TSharedPtr<PCGExData::FFacade> PathDataFacade = MakeShared<PCGExData::FFacade>(PathIO.ToSharedRef());

	TArray<FPCGPoint>& MutablePoints = PathIO->GetOut()->GetMutablePoints();
	MutablePoints.SetNumUninitialized(Cell->Nodes.Num());

	//const TArray<int32>& VtxPointIndices = Cluster->GetVtxPointIndices();
	for (int i = 0; i < Cell->Nodes.Num(); i++) { MutablePoints[i] = *Cluster->GetNodePoint(Cell->Nodes[i]); }

	ClusterProcessor->GetContext()->SeedAttributesToPathTags.Tag(SeedIndex, PathIO);
	ClusterProcessor->GetContext()->SeedForwardHandler->Forward(SeedIndex, PathDataFacade);

	if (Settings->bFlagDeadEnds)
	{
		const TSharedPtr<PCGExData::TBuffer<bool>> DeadEndBuffer = PathDataFacade->GetWritable(Settings->DeadEndAttributeName, false, true, PCGExData::EBufferInit::New);
		TArray<bool>& OutValues = *DeadEndBuffer->GetOutValues();
		for (int i = 0; i < Cell->Nodes.Num(); i++) { OutValues[i] = Cluster->GetNode(Cell->Nodes[i])->Adjacency.Num() == 1; }
	}

	if (!Cell->bIsClosedLoop) { if (Settings->bTagIfOpenPath) { PathIO->Tags->Add(Settings->IsOpenPathTag); } }
	else { if (Settings->bTagIfClosedLoop) { PathIO->Tags->Add(Settings->IsClosedLoopTag); } }

	PathDataFacade->Write(ClusterProcessor->GetAsyncManager());

	if (Settings->bOutputFilteredSeeds)
	{
		ClusterProcessor->GetContext()->SeedQuality[SeedIndex] = true;
		FPCGPoint SeedPoint = ClusterProcessor->GetContext()->SeedsDataFacade->Source->GetInPoint(SeedIndex);

		FVector Placement = RealSeedPosition;
		if (Settings->SeedPlacement == EPCGExGoodSeedPlacement::FirstPoint) { Placement = MutablePoints[0].Transform.GetLocation(); }
		else if (Settings->SeedPlacement == EPCGExGoodSeedPlacement::Centroid) { Placement = PCGEx::GetPointsCentroid(MutablePoints); }
		else if (Settings->SeedPlacement == EPCGExGoodSeedPlacement::PathBoundsCenter) { Placement = Cell->Bounds.GetCenter(); }
		else if (Settings->SeedPlacement == EPCGExGoodSeedPlacement::StartNode) { Placement = StartPosition; }

		if (Settings->SeedBounds != EPCGExGoodSeedBounds::Original)
		{
			SeedPoint.Transform.SetScale3D(FVector::OneVector);

			if (Settings->SeedBounds == EPCGExGoodSeedBounds::MatchPathResetQuat) { SeedPoint.Transform.SetRotation(FQuat::Identity); }

			SeedPoint.BoundsMin = Cell->Bounds.Min - Placement;
			SeedPoint.BoundsMax = Cell->Bounds.Max - Placement;
		}

		SeedPoint.Transform.SetLocation(Placement);
		ClusterProcessor->GetContext()->UdpatedSeedPoints[SeedIndex] = SeedPoint;
	}

	return true;
}

PCGEX_INITIALIZE_ELEMENT(FindContours)

bool FPCGExFindContoursElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindContours)

	PCGEX_FWD(ProjectionDetails)

	if (Settings->bFlagDeadEnds) { PCGEX_VALIDATE_NAME(Settings->DeadEndAttributeName); }

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
		Context->GoodSeeds->InitializeOutput(PCGExData::EIOInit::NewOutput);
		Context->GoodSeeds->GetOut()->GetMutablePoints().Reserve(NumSeeds);

		Context->BadSeeds = NewPointIO(SeedsPoints.ToSharedRef(), PCGExFindContours::OutputBadSeedsLabel);
		Context->BadSeeds->InitializeOutput(PCGExData::EIOInit::NewOutput);
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
				if (Settings->bFlagDeadEnds)
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
		TopologyConstraints = MakeShared<PCGExTopology::FCellConstraints>();

		TopologyConstraints->bDedupe = Settings->bDedupePaths;
		TopologyConstraints->bConcaveOnly = Settings->OutputType == EPCGExContourShapeTypeOutput::ConcaveOnly;
		TopologyConstraints->bConvexOnly = Settings->OutputType == EPCGExContourShapeTypeOutput::ConvexOnly;
		TopologyConstraints->bClosedLoopOnly = Settings->bKeepOnlyGracefulContours;
		TopologyConstraints->bKeepContoursWithDeadEnds = Settings->bKeepContoursWithDeadEnds;

		if (Settings->bOmitBelowPointCount) { TopologyConstraints->MinPointCount = Settings->MinPointCount; }
		if (Settings->bOmitAbovePointCount) { TopologyConstraints->MaxPointCount = Settings->MaxPointCount; }
		if (Settings->bOmitBelowBoundsSize) { TopologyConstraints->MinBoundsSize = Settings->MinBoundsSize; }
		if (Settings->bOmitAboveBoundsSize) { TopologyConstraints->MaxBoundsSize = Settings->MaxBoundsSize; }

		if (Settings->bUseOctreeSearch) { Cluster->RebuildOctree(Settings->SeedPicking.PickingMethod); }
		Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Edge); // We need edge octree anyway

		ExpandedNodes = Cluster->ExpandedNodes;

		if (!ExpandedNodes)
		{
			ExpandedNodes = Cluster->GetExpandedNodes(false);
			bBuildExpandedNodes = true;
			StartParallelLoopForRange(NumNodes);
		}

		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 Count)
	{
		*(ExpandedNodes->GetData() + Iteration) = PCGExCluster::FExpandedNode(Cluster, Iteration);
	}

	void FProcessor::CompleteWork()
	{
		for (int i = 0; i < Context->SeedsDataFacade->Source->GetNum(); i++)
		{
			AsyncManager->Start<FPCGExFindContourTask>(i, Context->Paths->Emplace_GetRef<UPCGPointData>(VtxDataFacade->Source, PCGExData::EIOInit::NoOutput), SharedThis(this));
		}
	}

	void FBatch::Process()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FindContours)

		// Project positions
		ProjectionDetails = Settings->ProjectionDetails;
		if (!ProjectionDetails.Init(Context, VtxDataFacade)) { return; }

		PCGEx::InitArray(ProjectedPositions, VtxDataFacade->GetNum());

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ProjectionTaskGroup)

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

		TBatch<FProcessor>::Process();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor)
	{
		ClusterProcessor->ProjectedPositions = &ProjectedPositions;
		TBatch<FProcessor>::PrepareSingle(ClusterProcessor);
		return true;
	}

	bool FPCGExFindContourTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		FPCGExFindContoursContext* Context = AsyncManager->GetContext<FPCGExFindContoursContext>();
		const FVector SeedPosition = Context->ProjectionDetails.Project(Context->SeedsDataFacade->Source->GetInPoint(TaskIndex).Transform.GetLocation(), TaskIndex);
		return Context->TryFindContours(PointIO, TaskIndex, SeedPosition, ClusterProcessor);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
