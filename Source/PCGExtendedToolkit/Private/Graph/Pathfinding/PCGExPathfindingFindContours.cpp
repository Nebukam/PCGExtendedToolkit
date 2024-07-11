// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingFindContours.h"

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
	return PinProperties;
}

PCGExData::EInit UPCGExFindContoursSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExFindContoursSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

bool FPCGExFindContoursContext::TryFindContours(PCGExData::FPointIO* PathIO, const int32 SeedIndex, PCGExFindContours::FProcessor* ClusterProcessor)
{
	const UPCGExFindContoursSettings* Settings = ClusterProcessor->LocalSettings;

	PCGExCluster::FCluster* Cluster = ClusterProcessor->Cluster;

	TArray<PCGExCluster::FExpandedNode*>* ExpandedNodes = ClusterProcessor->ExpandedNodes;
	TArray<PCGExCluster::FExpandedEdge*>* ExpandedEdges = ClusterProcessor->ExpandedEdges;

	const TArray<FVector>& Positions = *ClusterProcessor->ProjectedPositions;
	const TArray<PCGExCluster::FNode>& NodesRef = *Cluster->Nodes;

	const FVector Guide = ProjectedSeeds[SeedIndex];
	int32 StartNodeIndex = Cluster->FindClosestNode(Guide, Settings->SeedPicking.PickingMethod, 2);
	int32 NextEdge = Cluster->FindClosestEdge(StartNodeIndex, Guide);

	FBox PathBox = FBox(ForceInit);

	if (StartNodeIndex == -1
		|| NextEdge == -1
		// || (Cluster->Nodes->GetData() + StartNodeIndex)->Adjacency.Num() <= 1
	)
	{
		// Fail. Either single-node or single-edge cluster, or no connected edge
		return false;
	}

	const FVector SeedPosition = NodesRef[StartNodeIndex].Position;
	if (!Settings->SeedPicking.WithinDistance(SeedPosition, Guide))
	{
		// Fail. Not within radius.
		return false;
	}

	int32 PrevIndex = StartNodeIndex;
	int32 NextIndex = (*(ExpandedEdges->GetData() + NextEdge))->OtherNodeIndex(PrevIndex);


	const FVector A = (*(ExpandedNodes->GetData() + PrevIndex))->Node->Position;
	const FVector B = (*(ExpandedNodes->GetData() + NextIndex))->Node->Position;

	const double SanityAngle = PCGExMath::GetDegreesBetweenVectors((B - A).GetSafeNormal(), (B - Guide).GetSafeNormal());
	const bool bStartIsDeadEnd = (Cluster->Nodes->GetData() + StartNodeIndex)->Adjacency.Num() == 1;

	if (SanityAngle > 180 && !bStartIsDeadEnd)
	{
		// Swap search orientation
		PrevIndex = NextIndex;
		NextIndex = StartNodeIndex;
		StartNodeIndex = PrevIndex;
	}

	if (Settings->bDedupePaths)
	{
		uint64 StartHash = PCGEx::H64(PrevIndex, NextIndex);
		bool bAlreadyExists;
		{
			FWriteScopeLock WriteScopeLock(ClusterProcessor->UniquePathsLock);
			ClusterProcessor->UniquePathsStartPairs.Add(StartHash, &bAlreadyExists);
		}
		if (bAlreadyExists) { return false; }
	}

	TArray<int32> Path;
	Path.Add(PrevIndex);
	TSet<int32> Exclusions = {PrevIndex, NextIndex};
	TSet<uint64> SignedEdges;

	bool bIsConvex = true;
	int32 Sign = 0;

	PathBox += (*(ExpandedNodes->GetData() + PrevIndex))->Node->Position;

	bool bGracefullyClosed = false;
	while (NextIndex != -1)
	{
		double BestAngle = -1;
		int32 NextBest = -1;

		bool bEdgeAlreadyExists;
		SignedEdges.Add(PCGEx::H64(PrevIndex, NextIndex), &bEdgeAlreadyExists);
		if (bEdgeAlreadyExists) { break; }

		Path.Add(NextIndex);
		PCGExCluster::FExpandedNode* Current = *(ExpandedNodes->GetData() + NextIndex);

		PathBox += Current->Node->Position;

		//if (Current->Neighbors.Num() <= 1) { break; }
		if (Current->Neighbors.Num() == 1 && Settings->bDuplicateDeadEndPoints) { Path.Add(NextIndex); }

		const FVector Origin = Positions[(Cluster->Nodes->GetData() + NextIndex)->PointIndex];
		const FVector GuideDir = (Origin - Positions[(Cluster->Nodes->GetData() + PrevIndex)->PointIndex]).GetSafeNormal();

		if (Current->Neighbors.Num() > 1) { Exclusions.Add(PrevIndex); }

		bool bHasAdjacencyToStart = false;
		for (const PCGExCluster::FExpandedNeighbor& N : Current->Neighbors)
		{
			const int32 NeighborIndex = N.Node->NodeIndex;

			if (NeighborIndex == StartNodeIndex) { bHasAdjacencyToStart = true; }
			if (Exclusions.Contains(NeighborIndex)) { continue; }

			const FVector OtherDir = (Origin - Positions[(Cluster->Nodes->GetData() + NeighborIndex)->PointIndex]).GetSafeNormal();
			const double Angle = PCGExMath::GetDegreesBetweenVectors(OtherDir, GuideDir);

			if (Angle > BestAngle)
			{
				BestAngle = Angle;
				NextBest = NeighborIndex;
			}
		}

		Exclusions.Empty();

		if (NextBest == StartNodeIndex)
		{
			bGracefullyClosed = true;
			NextBest = -1;
		}

		if (NextBest != -1)
		{
			if (Settings->OutputType != EPCGExContourShapeTypeOutput::Both && Path.Num() > 2)
			{
				PCGExMath::CheckConvex(
					(Cluster->Nodes->GetData() + Path.Last(2))->Position,
					(Cluster->Nodes->GetData() + Path.Last(1))->Position,
					(Cluster->Nodes->GetData() + Path.Last())->Position,
					bIsConvex, Sign);

				if (!bIsConvex && Settings->OutputType == EPCGExContourShapeTypeOutput::ConvexOnly) { return false; }
			}

			PrevIndex = NextIndex;
			NextIndex = NextBest;
		}
		else
		{
			if (bHasAdjacencyToStart) { bGracefullyClosed = true; }
			NextIndex = -1;
		}
	}

	SignedEdges.Empty();

	if ((Settings->bKeepOnlyGracefulContours && !bGracefullyClosed) ||
		(bIsConvex && Settings->OutputType == EPCGExContourShapeTypeOutput::ConcaveOnly))
	{
		return false;
	}

	if (Settings->bDedupePaths)
	{
		uint32 BoxHash = HashCombineFast(GetTypeHash(PathBox.Min), GetTypeHash(PathBox.Max));
		bool bAlreadyExists;
		{
			FWriteScopeLock WriteScopeLock(ClusterProcessor->UniquePathsLock);
			ClusterProcessor->UniquePathsBounds.Add(BoxHash, &bAlreadyExists);
		}
		if (bAlreadyExists) { return false; }
	}

	PCGExGraph::CleanupClusterTags(PathIO, true);
	PCGExGraph::CleanupVtxData(PathIO);

	PCGExData::FFacade* PathDataFacade = new PCGExData::FFacade(PathIO);

	TArray<FPCGPoint>& MutablePoints = PathIO->GetOut()->GetMutablePoints();
	const TArray<FPCGPoint>& OriginPoints = PathIO->GetIn()->GetPoints();
	MutablePoints.SetNumUninitialized(Path.Num());

	const TArray<int32>& VtxPointIndices = Cluster->GetVtxPointIndices();
	for (int i = 0; i < Path.Num(); i++) { MutablePoints[i] = OriginPoints[VtxPointIndices[Path[i]]]; }

	const FPCGExFindContoursContext* TypedContext = static_cast<FPCGExFindContoursContext*>(ClusterProcessor->Context);
	TypedContext->SeedAttributesToPathTags.Tag(SeedIndex, PathIO);
	TypedContext->SeedForwardHandler->Forward(SeedIndex, PathDataFacade);

	if (Settings->bFlagDeadEnds)
	{
		PathIO->CreateOutKeys();
		PCGEx::TFAttributeWriter<bool>* DeadEndWriter = new PCGEx::TFAttributeWriter<bool>(Settings->DeadEndAttributeName, false, false, true);
		DeadEndWriter->BindAndSetNumUninitialized(PathIO);
		for (int i = 0; i < Path.Num(); i++) { DeadEndWriter->Values[i] = (Cluster->Nodes->GetData() + Path[i])->Adjacency.Num() == 1; }
		PCGEX_ASYNC_WRITE_DELETE(ClusterProcessor->AsyncManagerPtr, DeadEndWriter)
	}

	if (Sign != 0)
	{
		if (Settings->bTagConcave && !bIsConvex) { PathIO->Tags->RawTags.Add(Settings->ConcaveTag); }
		if (Settings->bTagConvex && bIsConvex) { PathIO->Tags->RawTags.Add(Settings->ConvexTag); }
	}

	PathDataFacade->Write(ClusterProcessor->AsyncManagerPtr, true);
	PCGEX_DELETE(PathDataFacade)

	return true;
}

PCGEX_INITIALIZE_ELEMENT(FindContours)

FPCGExFindContoursContext::~FPCGExFindContoursContext()
{
	PCGEX_TERMINATE_ASYNC

	if (SeedsDataFacade) { PCGEX_DELETE(SeedsDataFacade->Source) }
	PCGEX_DELETE(SeedsDataFacade)

	PCGEX_DELETE(Paths)

	SeedAttributesToPathTags.Cleanup();
	PCGEX_DELETE(SeedForwardHandler)

	ProjectedSeeds.Empty();
}


bool FPCGExFindContoursElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindContours)

	PCGEX_FWD(ProjectionDetails)

	if (Settings->bFlagDeadEnds) { PCGEX_VALIDATE_NAME(Settings->DeadEndAttributeName); }

	PCGExData::FPointIO* SeedsPoints = PCGExData::TryGetSingleInput(Context, PCGExGraph::SourceSeedsLabel, true);
	if (!SeedsPoints) { return false; }
	Context->SeedsDataFacade = new PCGExData::FFacade(SeedsPoints);

	if (!Context->ProjectionDetails.Init(Context, Context->SeedsDataFacade)) { return false; }

	PCGEX_FWD(SeedAttributesToPathTags)
	if (!Context->SeedAttributesToPathTags.Init(Context, Context->SeedsDataFacade)) { return false; }
	Context->SeedForwardHandler = Settings->SeedForwarding.GetHandler(Context->SeedsDataFacade);

	Context->Paths = new PCGExData::FPointIOCollection();
	Context->Paths->DefaultOutputLabel = PCGExGraph::OutputPathsLabel;

	return true;
}

bool FPCGExFindContoursElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindContoursElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FindContours)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ProcessingTargets);
	}

	if (Context->IsState(PCGExMT::State_ProcessingTargets))
	{
		const TArray<FPCGPoint>& Seeds = Context->SeedsDataFacade->GetIn()->GetPoints();

		auto Initialize = [&]()
		{
			Context->ProjectedSeeds.SetNumUninitialized(Seeds.Num());
		};

		auto ProjectSeed = [&](const int32 Index)
		{
			Context->ProjectedSeeds[Index] = Context->ProjectionDetails.Project(Seeds[Index].Transform.GetLocation(), Index);
		};

		if (!Context->Process(Initialize, ProjectSeed, Seeds.Num())) { return false; }

		if (!Context->StartProcessingClusters<PCGExFindContours::FBatch>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExFindContours::FBatch* NewBatch)
			{
				if (Settings->bFlagDeadEnds)
				{
					NewBatch->bRequiresWriteStep = true;
					NewBatch->bWriteVtxDataFacade = true;
				}
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	if (Context->IsDone())
	{
		Context->Paths->OutputTo(Context);
	}

	return Context->TryComplete();
}


namespace PCGExFindContours
{
	FProcessor::~FProcessor()
	{
		if (bBuildExpandedNodes) { PCGEX_DELETE_TARRAY_FULL(ExpandedNodes) }
		UniquePathsBounds.Empty();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FindContours)

		LocalSettings = Settings;

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		if (Settings->bUseOctreeSearch) { Cluster->RebuildOctree(Settings->SeedPicking.PickingMethod); }
		Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Edge); // We need edge octree anyway

		ExpandedNodes = Cluster->ExpandedNodes;
		ExpandedEdges = Cluster->GetExpandedEdges(true);

		if (!ExpandedNodes)
		{
			ExpandedNodes = Cluster->GetExpandedNodes(false);
			bBuildExpandedNodes = true;
			StartParallelLoopForRange(NumNodes);
		}

		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration)
	{
		(*ExpandedNodes)[Iteration] = new PCGExCluster::FExpandedNode(Cluster, Iteration);
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FindContours)

		if (IsTrivial())
		{
			for (int i = 0; i < TypedContext->ProjectedSeeds.Num(); i++)
			{
				TypedContext->TryFindContours(TypedContext->Paths->Emplace_GetRef<UPCGPointData>(VtxIO, PCGExData::EInit::NewOutput), i, this);
			}
		}
		else
		{
			for (int i = 0; i < TypedContext->ProjectedSeeds.Num(); i++)
			{
				AsyncManagerPtr->Start<FPCGExFindContourTask>(i, TypedContext->Paths->Emplace_GetRef<UPCGPointData>(VtxIO, PCGExData::EInit::NewOutput), this);
			}
		}
	}

	void FBatch::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FindContours)

		// Project positions
		ProjectionDetails = Settings->ProjectionDetails;
		if (!ProjectionDetails.Init(Context, VtxDataFacade)) { return; }

		PCGEX_SET_NUM_UNINITIALIZED(ProjectedPositions, VtxIO->GetNum())

		ProjectionTaskGroup = AsyncManager->CreateGroup();
		ProjectionTaskGroup->StartRanges(
			[&](const int32 Index)
			{
				ProjectedPositions[Index] = ProjectionDetails.ProjectFlat(VtxIO->GetInPoint(Index).Transform.GetLocation(), Index);
			},
			VtxIO->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchIteration());

		TBatch<FProcessor>::Process(AsyncManager);
	}

	bool FBatch::PrepareSingle(FProcessor* ClusterProcessor)
	{
		ClusterProcessor->ProjectedPositions = &ProjectedPositions;
		TBatch<FProcessor>::PrepareSingle(ClusterProcessor);
		return true;
	}

	bool FProjectRangeTask::ExecuteTask()
	{
		for (int i = 0; i < NumIterations; i++)
		{
			const int32 Index = TaskIndex + i;
			Batch->ProjectedPositions[Index] = Batch->ProjectionDetails.ProjectFlat(Batch->VtxIO->GetInPoint(Index).Transform.GetLocation(), Index);
		}
		return true;
	}

	bool FPCGExFindContourTask::ExecuteTask()
	{
		FPCGExFindContoursContext* Context = Manager->GetContext<FPCGExFindContoursContext>();
		return Context->TryFindContours(PointIO, TaskIndex, ClusterProcessor);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
