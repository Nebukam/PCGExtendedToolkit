// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingGrowPaths.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"
#include "Algo/Reverse.h"


#define LOCTEXT_NAMESPACE "PCGExPathfindingGrowPathsElement"
#define PCGEX_NAMESPACE PathfindingGrowPaths

#define PCGEX_GROWTH_GRAB(_CONTEXT, _TARGET, _SOURCE, _TYPE, _ATTRIBUTE) \
_TARGET = _SOURCE->GetBroadcaster<_TYPE>(_ATTRIBUTE); \
if (!_TARGET){	PCGE_LOG_C(Error, GraphAndLog, _CONTEXT, FTEXT("Missing specified " #_ATTRIBUTE " attribute."));	return false; }

#if WITH_EDITOR
void UPCGExPathfindingGrowPathsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

namespace PCGExGrowPaths
{
	FGrowth::FGrowth(const TSharedPtr<FProcessor>& InProcessor,
	                 const int32 InMaxIterations,
	                 const int32 InLastGrowthIndex,
	                 const FVector& InGrowthDirection):
		Processor(InProcessor),
		MaxIterations(InMaxIterations),
		LastGrowthIndex(InLastGrowthIndex),
		GrowthDirection(InGrowthDirection)
	{
		SoftMaxIterations = InMaxIterations;
		Path.Reserve(MaxIterations);
		Path.Add(InLastGrowthIndex);
		Init();
	}

	int32 FGrowth::FindNextGrowthNodeIndex()
	{
		if (Iteration + 1 > SoftMaxIterations)
		{
			NextGrowthIndex = -1;
			return NextGrowthIndex;
		}

		const TArray<PCGExCluster::FNode>& NodesRef = *Processor->Cluster->Nodes;
		const TArray<PCGExGraph::FIndexedEdge>& EdgesRef = *Processor->Cluster->Edges;

		const PCGExCluster::FNode& CurrentNode = NodesRef[LastGrowthIndex];

		double BestScore = MAX_dbl;
		NextGrowthIndex = -1;
		NextGrowthEdgeIndex = -1;

		for (const uint64 AdjacencyHash : CurrentNode.Adjacency)
		{
			uint32 NeighborIndex;
			uint32 EdgeIndex;
			PCGEx::H64(AdjacencyHash, NeighborIndex, EdgeIndex);

			const PCGExCluster::FNode& OtherNode = NodesRef[NeighborIndex];

			if (Processor->GetSettings()->bUseNoGrowth)
			{
				bool bNoGrowth = Processor->NoGrowth ? Processor->NoGrowth->Read(OtherNode.PointIndex) : Processor->GetSettings()->bInvertNoGrowth;
				if (Processor->GetSettings()->bInvertNoGrowth) { bNoGrowth = !bNoGrowth; }

				if (bNoGrowth) { continue; }
			}

			if (Path.Contains(NeighborIndex)) { continue; }

			/*
			// TODO : Implement
			if (Settings->VisitedStopThreshold > 0 && Context->GlobalExtraWeights &&
				Context->GlobalExtraWeights->GetExtraWeight(AdjacentNodeIndex, EdgeIndex) > Settings->VisitedStopThreshold)
			{
				continue;
			}
			*/

			if (const double Score = GetGrowthScore(CurrentNode, OtherNode, EdgesRef[EdgeIndex]); Score < BestScore)
			{
				BestScore = Score;
				NextGrowthIndex = OtherNode.NodeIndex;
				NextGrowthEdgeIndex = EdgeIndex;
			}
		}

		return NextGrowthIndex;
	}

	bool FGrowth::Grow()
	{
		if (NextGrowthIndex <= -1 || Path.Contains(NextGrowthIndex)) { return false; }

		TravelStack->Set(NextGrowthIndex, PCGEx::NH64(LastGrowthIndex, NextGrowthEdgeIndex));
		
		const TArray<PCGExCluster::FNode>& NodesRef = *Processor->Cluster->Nodes;
		const TArray<PCGExGraph::FIndexedEdge>& EdgesRef = *Processor->Cluster->Edges;

		const PCGExCluster::FNode& CurrentNode = NodesRef[LastGrowthIndex];
		const PCGExCluster::FNode& NextNode = NodesRef[NextGrowthIndex];

		Metrics.Add(Processor->Cluster->GetPos(NextNode));
		if (MaxDistance > 0 && Metrics.Length > MaxDistance) { return false; }

		
		Processor->HeuristicsHandler->FeedbackScore(NextNode, EdgesRef[NextGrowthEdgeIndex]);

		Iteration++;
		Path.Add(NextGrowthIndex);
		LastGrowthIndex = NextGrowthIndex;

		if (Processor->GetSettings()->NumIterations == EPCGExGrowthValueSource::VtxAttribute)
		{
			if (Processor->GetSettings()->NumIterationsUpdateMode == EPCGExGrowthUpdateMode::SetEachIteration)
			{
				SoftMaxIterations = Processor->NumIterations->Read(NextNode.PointIndex);
			}
			else if (Processor->GetSettings()->NumIterationsUpdateMode == EPCGExGrowthUpdateMode::AddEachIteration)
			{
				SoftMaxIterations += Processor->NumIterations->Read(NextNode.PointIndex);
			}
		}

		if (Processor->GetSettings()->GrowthDirection == EPCGExGrowthValueSource::VtxAttribute)
		{
			if (Processor->GetSettings()->GrowthDirectionUpdateMode == EPCGExGrowthUpdateMode::SetEachIteration)
			{
				GrowthDirection = Processor->GrowthDirection->Read(NextNode.PointIndex);
			}
			else if (Processor->GetSettings()->GrowthDirectionUpdateMode == EPCGExGrowthUpdateMode::AddEachIteration)
			{
				GrowthDirection = (GrowthDirection + Processor->GrowthDirection->Read(NextNode.PointIndex)).GetSafeNormal();
			}
		}

		Processor->Cluster->NodePositions[GoalNode->NodeIndex] = Processor->Cluster->GetPos(NextNode) + GrowthDirection * 10000;

		if (Processor->GetSettings()->bUseGrowthStop)
		{
			bool bStopGrowth = Processor->GrowthStop ? Processor->GrowthStop->Read(NextNode.PointIndex) : Processor->GetSettings()->bInvertGrowthStop;
			if (Processor->GetSettings()->bInvertGrowthStop) { bStopGrowth = !bStopGrowth; }
			if (bStopGrowth) { SoftMaxIterations = -1; }
		}

		return true;
	}

	void FGrowth::Write()
	{
		const TSharedPtr<PCGExData::FPointIO> VtxIO = Processor->Cluster->VtxIO.Pin();
		const TSharedPtr<PCGExData::FPointIO> PathIO = Processor->GetContext()->OutputPaths->Emplace_GetRef<UPCGPointData>(VtxIO->GetIn(), PCGExData::EInit::NewOutput);
		const TSharedPtr<PCGExData::FFacade> PathDataFacade = MakeShared<PCGExData::FFacade>(PathIO.ToSharedRef());

		UPCGPointData* OutData = PathIO->GetOut();

		PCGExGraph::CleanupVtxData(PathIO);

		TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
		const TArray<FPCGPoint>& InPoints = VtxIO->GetIn()->GetPoints();

		MutablePoints.Reserve(Path.Num());

		for (const int32 VtxIndex : Path) { MutablePoints.Add(*Processor->Cluster->GetNodePoint(VtxIndex)); }

		PathIO->Tags->Append(VtxIO->Tags.ToSharedRef());

		Processor->GetContext()->SeedAttributesToPathTags.Tag(SeedPointIndex, PathIO);
		Processor->GetContext()->SeedForwardHandler->Forward(SeedPointIndex, PathDataFacade);

		PathDataFacade->Write(Processor->AsyncManager);
	}

	void FGrowth::Init()
	{
		SeedNode = &(*Processor->Cluster->Nodes)[LastGrowthIndex];
		GoalNode = MakeUnique<PCGExCluster::FNode>();
		GoalNode->NodeIndex = Processor->Cluster->NodePositions.Add(Processor->Cluster->GetPos(SeedNode) + GrowthDirection * 10000);
		Metrics.Reset(Processor->Cluster->GetPos(SeedNode));
		TravelStack = PCGEx::NewHashLookup<PCGEx::FMapHashLookup>(PCGEx::NH64(-1, -1), 0);
	}

	double FGrowth::GetGrowthScore(const PCGExCluster::FNode& From, const PCGExCluster::FNode& To, const PCGExGraph::FIndexedEdge& Edge) const
	{
		return Processor->HeuristicsHandler->GetEdgeScore(From, To, Edge, *SeedNode, *GoalNode.Get(), nullptr, TravelStack);
	}
}

TArray<FPCGPinProperties> UPCGExPathfindingGrowPathsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGExGraph::SourceSeedsLabel, "Seed points to start growth from.", Required, {})
	PCGEX_PIN_PARAMS(PCGExGraph::SourceHeuristicsLabel, "Heuristics.", Normal, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPathfindingGrowPathsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputPathsLabel, "Paths output.", Required, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(PathfindingGrowPaths)

bool FPCGExPathfindingGrowPathsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingGrowPaths)

	const TSharedPtr<PCGExData::FPointIO> SeedsPoints = PCGExData::TryGetSingleInput(Context, PCGExGraph::SourceSeedsLabel, true);
	if (!SeedsPoints) { return false; }

	Context->SeedsDataFacade = MakeShared<PCGExData::FFacade>(SeedsPoints.ToSharedRef());
	Context->OutputPaths = MakeShared<PCGExData::FPointIOCollection>(Context);

	if (Settings->NumIterations == EPCGExGrowthValueSource::SeedAttribute)
	{
		PCGEX_GROWTH_GRAB(Context, Context->NumIterations, Context->SeedsDataFacade, int32, Settings->NumIterationsAttribute)
	}

	if (Settings->SeedNumBranches == EPCGExGrowthValueSource::SeedAttribute)
	{
		PCGEX_GROWTH_GRAB(Context, Context->NumBranches, Context->SeedsDataFacade, int32, Settings->NumBranchesAttribute)
	}

	if (Settings->GrowthDirection == EPCGExGrowthValueSource::SeedAttribute)
	{
		PCGEX_GROWTH_GRAB(Context, Context->GrowthDirection, Context->SeedsDataFacade, FVector, Settings->GrowthDirectionAttribute)
	}

	if (Settings->GrowthMaxDistance == EPCGExGrowthValueSource::SeedAttribute)
	{
		PCGEX_GROWTH_GRAB(Context, Context->GrowthMaxDistance, Context->SeedsDataFacade, double, Settings->GrowthMaxDistanceAttribute)
	}

	PCGEX_FWD(SeedAttributesToPathTags)

	if (!Context->SeedAttributesToPathTags.Init(Context, Context->SeedsDataFacade)) { return false; }
	Context->SeedForwardHandler = Settings->SeedForwarding.GetHandler(Context->SeedsDataFacade);

	return true;
}

bool FPCGExPathfindingGrowPathsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfindingGrowPathsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingGrowPaths)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatchWithHeuristics<PCGExGrowPaths::FProcessor>>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::TBatchWithHeuristics<PCGExGrowPaths::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	Context->OutputPaths->StageOutputs();

	return Context->TryComplete();
}


namespace PCGExGrowPaths
{
	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExGrowPaths::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		// Prepare getters

		if (Settings->NumIterations == EPCGExGrowthValueSource::VtxAttribute)
		{
			PCGEX_GROWTH_GRAB(ExecutionContext, NumIterations, VtxDataFacade, int32, Settings->NumIterationsAttribute)
		}

		if (Settings->SeedNumBranches == EPCGExGrowthValueSource::VtxAttribute)
		{
			PCGEX_GROWTH_GRAB(ExecutionContext, NumBranches, VtxDataFacade, int32, Settings->NumBranchesAttribute)
		}

		if (Settings->GrowthDirection == EPCGExGrowthValueSource::VtxAttribute)
		{
			PCGEX_GROWTH_GRAB(ExecutionContext, GrowthDirection, VtxDataFacade, FVector, Settings->GrowthDirectionAttribute)
		}

		if (Settings->GrowthMaxDistance == EPCGExGrowthValueSource::VtxAttribute)
		{
			PCGEX_GROWTH_GRAB(ExecutionContext, GrowthMaxDistance, VtxDataFacade, double, Settings->GrowthMaxDistanceAttribute)
		}

		GrowthStop = Settings->bUseGrowthStop ? VtxDataFacade->GetBroadcaster<bool>(Settings->GrowthStopAttribute) : nullptr;
		NoGrowth = Settings->bUseNoGrowth ? VtxDataFacade->GetBroadcaster<bool>(Settings->NoGrowthAttribute) : nullptr;

		if (Settings->bUseOctreeSearch) { Cluster->RebuildOctree(Settings->SeedPicking.PickingMethod); }

		// Prepare growth points

		// Find all growth points
		const int32 SeedCount = Context->SeedsDataFacade->Source->GetNum();
		for (int i = 0; i < SeedCount; i++)
		{
			const FVector SeedPosition = Context->SeedsDataFacade->Source->GetInPoint(i).Transform.GetLocation();
			const int32 NodeIndex = Cluster->FindClosestNode(SeedPosition, Settings->SeedPicking.PickingMethod, 1);

			if (NodeIndex == -1) { continue; }

			const PCGExCluster::FNode& Node = (*Cluster->Nodes)[NodeIndex];
			if (!Settings->SeedPicking.WithinDistance(Cluster->GetPos(Node), SeedPosition) ||
				Node.Adjacency.IsEmpty()) { continue; }

			double StartNumIterations;
			double StartGrowthNumBranches;
			FVector StartGrowthDirection = FVector::UpVector;
			double StartGrowthMaxDistance;

			switch (Settings->SeedNumBranches)
			{
			default: ;
			case EPCGExGrowthValueSource::Constant:
				StartGrowthNumBranches = Settings->NumBranchesConstant;
				break;
			case EPCGExGrowthValueSource::SeedAttribute:
				StartGrowthNumBranches = Context->NumBranches->Read(i);
				break;
			case EPCGExGrowthValueSource::VtxAttribute:
				StartGrowthNumBranches = NumBranches->Read(Node.PointIndex);
				break;
			}

			switch (Settings->NumIterations)
			{
			default: ;
			case EPCGExGrowthValueSource::Constant:
				StartNumIterations = Settings->NumIterationsConstant;
				break;
			case EPCGExGrowthValueSource::SeedAttribute:
				StartNumIterations = Context->NumIterations->Read(i);
				break;
			case EPCGExGrowthValueSource::VtxAttribute:
				StartNumIterations = NumIterations->Read(Node.PointIndex);
				break;
			}

			switch (Settings->GrowthMaxDistance)
			{
			default: ;
			case EPCGExGrowthValueSource::Constant:
				StartGrowthMaxDistance = Settings->GrowthMaxDistanceConstant;
				break;
			case EPCGExGrowthValueSource::SeedAttribute:
				StartGrowthMaxDistance = Context->GrowthMaxDistance->Read(i);
				break;
			case EPCGExGrowthValueSource::VtxAttribute:
				StartGrowthMaxDistance = GrowthMaxDistance->Read(Node.PointIndex);
				break;
			}

			switch (Settings->GrowthDirection)
			{
			default: ;
			case EPCGExGrowthValueSource::Constant:
				StartGrowthDirection = Settings->GrowthDirectionConstant;
				break;
			case EPCGExGrowthValueSource::SeedAttribute:
				StartGrowthDirection = Context->GrowthDirection->Read(i);
				break;
			case EPCGExGrowthValueSource::VtxAttribute:
				StartGrowthDirection = GrowthDirection->Read(Node.PointIndex);
				break;
			}

			if (StartGrowthNumBranches <= 0 || StartNumIterations <= 0) { continue; }

			if (Settings->SeedNumBranchesMean == EPCGExMeanMeasure::Relative)
			{
				StartGrowthNumBranches = FMath::Max(1, static_cast<double>(Node.Adjacency.Num()) * StartGrowthNumBranches);
			}

			for (int j = 0; j < StartGrowthNumBranches; j++)
			{
				TSharedPtr<FGrowth> NewGrowth = MakeShared<FGrowth>(SharedThis(this), StartNumIterations, Node.NodeIndex, StartGrowthDirection);
				NewGrowth->MaxDistance = StartGrowthMaxDistance;
				NewGrowth->SeedPointIndex = i;

				if (!(NewGrowth->FindNextGrowthNodeIndex() != -1 && NewGrowth->Grow())) { continue; }

				Growths.Add(NewGrowth);
				QueuedGrowths.Add(NewGrowth);
			}
		}

		if (IsTrivial()) { Grow(); }
		else { AsyncManager->Start<FGrowTask>(BatchIndex, nullptr, SharedThis(this)); }

		return true;
	}

	void FProcessor::CompleteWork()
	{
		for (const TSharedPtr<FGrowth>& Growth : Growths) { Growth->Write(); }
	}

	void FProcessor::Grow()
	{
		if (Settings->GrowthMode == EPCGExGrowthIterationMode::Parallel)
		{
			for (const TSharedPtr<FGrowth>& Growth : QueuedGrowths)
			{
				while (Growth->FindNextGrowthNodeIndex() != -1 && Growth->Grow())
				{
				}
			}

			QueuedGrowths.Empty();
		}
		else
		{
			while (!QueuedGrowths.IsEmpty())
			{
				for (int i = 0; i < QueuedGrowths.Num(); i++)
				{
					const TSharedPtr<FGrowth>& Growth = QueuedGrowths[i];

					Growth->FindNextGrowthNodeIndex();

					if (!Growth->Grow())
					{
						QueuedGrowths.RemoveAt(i);
						i--;
					}
				}
			}
		}
	}

	bool FGrowTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		Processor->Grow();
		return true;
	}
}

#undef PCGEX_GROWTH_GRAB

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
