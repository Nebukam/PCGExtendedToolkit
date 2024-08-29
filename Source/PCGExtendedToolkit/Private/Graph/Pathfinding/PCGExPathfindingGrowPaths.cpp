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
	FGrowth::FGrowth(const FProcessor* InProcessor, const UPCGExPathfindingGrowPathsSettings* InSettings, const int32 InMaxIterations, const int32 InLastGrowthIndex, const FVector& InGrowthDirection):
		Processor(InProcessor),
		Context(static_cast<FPCGExPathfindingGrowPathsContext*>(Processor->Context)),
		Settings(InSettings),
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

		double BestScore = TNumericLimits<double>::Max();
		NextGrowthIndex = -1;

		for (const uint64 AdjacencyHash : CurrentNode.Adjacency)
		{
			uint32 NeighborIndex;
			uint32 EdgeIndex;
			PCGEx::H64(AdjacencyHash, NeighborIndex, EdgeIndex);

			const PCGExCluster::FNode& OtherNode = NodesRef[NeighborIndex];

			if (Settings->bUseNoGrowth)
			{
				bool bNoGrowth = Processor->NoGrowth ? Processor->NoGrowth->Values[OtherNode.PointIndex] : Settings->bInvertNoGrowth;
				if (Settings->bInvertNoGrowth) { bNoGrowth = !bNoGrowth; }

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

			if (const double Score = GetGrowthScore(
					CurrentNode, OtherNode, EdgesRef[EdgeIndex]);
				Score < BestScore)
			{
				BestScore = Score;
				NextGrowthIndex = OtherNode.NodeIndex;
			}
		}


		return NextGrowthIndex;
	}

	bool FGrowth::Grow()
	{
		if (NextGrowthIndex <= -1 || Path.Contains(NextGrowthIndex)) { return false; }

		const TArray<PCGExCluster::FNode>& NodesRef = *Processor->Cluster->Nodes;
		const TArray<PCGExGraph::FIndexedEdge>& EdgesRef = *Processor->Cluster->Edges;

		const PCGExCluster::FNode& CurrentNode = NodesRef[LastGrowthIndex];
		const PCGExCluster::FNode& NextNode = NodesRef[NextGrowthIndex];

		Metrics.Add(Processor->Cluster->GetPos(NextNode));
		if (MaxDistance > 0 && Metrics.Length > MaxDistance) { return false; }

		Processor->HeuristicsHandler->FeedbackScore(NextNode, EdgesRef[CurrentNode.GetEdgeIndex(NextNode.NodeIndex)]);

		Iteration++;
		Path.Add(NextGrowthIndex);
		LastGrowthIndex = NextGrowthIndex;

		if (Settings->NumIterations == EPCGExGrowthValueSource::VtxAttribute)
		{
			if (Settings->NumIterationsUpdateMode == EPCGExGrowthUpdateMode::SetEachIteration)
			{
				SoftMaxIterations = Processor->NumIterations->Values[NextNode.PointIndex];
			}
			else if (Settings->NumIterationsUpdateMode == EPCGExGrowthUpdateMode::AddEachIteration)
			{
				SoftMaxIterations += Processor->NumIterations->Values[NextNode.PointIndex];
			}
		}

		if (Settings->GrowthDirection == EPCGExGrowthValueSource::VtxAttribute)
		{
			if (Settings->GrowthDirectionUpdateMode == EPCGExGrowthUpdateMode::SetEachIteration)
			{
				GrowthDirection = Processor->GrowthDirection->Values[NextNode.PointIndex];
			}
			else if (Settings->GrowthDirectionUpdateMode == EPCGExGrowthUpdateMode::AddEachIteration)
			{
				GrowthDirection = (GrowthDirection + Processor->GrowthDirection->Values[NextNode.PointIndex]).GetSafeNormal();
			}
		}

		Processor->Cluster->NodePositions[GoalNode->NodeIndex] = Processor->Cluster->GetPos(NextNode) + GrowthDirection * 10000;

		if (Settings->bUseGrowthStop)
		{
			bool bStopGrowth = Processor->GrowthStop ? Processor->GrowthStop->Values[NextNode.PointIndex] : Settings->bInvertGrowthStop;
			if (Settings->bInvertGrowthStop) { bStopGrowth = !bStopGrowth; }
			if (bStopGrowth) { SoftMaxIterations = -1; }
		}

		return true;
	}

	void FGrowth::Write()
	{
		const PCGExData::FPointIO* VtxIO = Processor->Cluster->VtxIO;
		PCGExData::FPointIO* PathIO = Context->OutputPaths->Emplace_GetRef<UPCGPointData>(VtxIO->GetIn(), PCGExData::EInit::NewOutput);
		PCGExData::FFacade* PathDataFacade = new PCGExData::FFacade(PathIO);

		UPCGPointData* OutData = PathIO->GetOut();

		PCGExGraph::CleanupVtxData(PathIO);

		TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
		const TArray<FPCGPoint>& InPoints = VtxIO->GetIn()->GetPoints();

		MutablePoints.Reserve(Path.Num());

		const TArray<int32>& VtxPointIndices = Processor->Cluster->GetVtxPointIndices();
		for (const int32 VtxIndex : Path) { MutablePoints.Add(InPoints[VtxPointIndices[VtxIndex]]); }

		PathIO->Tags->Append(VtxIO->Tags);

		Context->SeedAttributesToPathTags.Tag(SeedPointIndex, PathIO);
		Context->SeedForwardHandler->Forward(SeedPointIndex, PathDataFacade);

		PathDataFacade->Write(Processor->AsyncManagerPtr, true);
		PCGEX_DELETE(PathDataFacade)
	}

	void FGrowth::Init()
	{
		SeedNode = &(*Processor->Cluster->Nodes)[LastGrowthIndex];
		GoalNode = new PCGExCluster::FNode();
		GoalNode->NodeIndex = Processor->Cluster->NodePositions.Add(Processor->Cluster->GetPos(SeedNode) + GrowthDirection * 100);
		Metrics.Reset(Processor->Cluster->GetPos(SeedNode));
	}

	double FGrowth::GetGrowthScore(const PCGExCluster::FNode& From, const PCGExCluster::FNode& To, const PCGExGraph::FIndexedEdge& Edge) const
	{
		return Processor->HeuristicsHandler->GetEdgeScore(From, To, Edge, *SeedNode, *GoalNode);
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

FPCGExPathfindingGrowPathsContext::~FPCGExPathfindingGrowPathsContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE_FACADE_AND_SOURCE(SeedsDataFacade)
	PCGEX_DELETE(OutputPaths)

	SeedAttributesToPathTags.Cleanup();
}

bool FPCGExPathfindingGrowPathsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingGrowPaths)

	PCGExData::FPointIO* SeedsPoints;

	SeedsPoints = PCGExData::TryGetSingleInput(Context, PCGExGraph::SourceSeedsLabel, true);
	if (!SeedsPoints) { return false; }

	Context->SeedsDataFacade = new PCGExData::FFacade(SeedsPoints);

	Context->OutputPaths = new PCGExData::FPointIOCollection(Context);

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

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatchWithHeuristics<PCGExGrowPaths::FProcessor>>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExClusterMT::TBatchWithHeuristics<PCGExGrowPaths::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	Context->OutputPaths->OutputToContext();

	return Context->TryComplete();
}


namespace PCGExGrowPaths
{
	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExGrowPaths::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathfindingGrowPaths)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		// Prepare getters

		if (Settings->NumIterations == EPCGExGrowthValueSource::VtxAttribute)
		{
			PCGEX_GROWTH_GRAB(Context, NumIterations, VtxDataFacade, int32, Settings->NumIterationsAttribute)
		}

		if (Settings->SeedNumBranches == EPCGExGrowthValueSource::VtxAttribute)
		{
			PCGEX_GROWTH_GRAB(Context, NumBranches, VtxDataFacade, int32, Settings->NumBranchesAttribute)
		}

		if (Settings->GrowthDirection == EPCGExGrowthValueSource::VtxAttribute)
		{
			PCGEX_GROWTH_GRAB(Context, GrowthDirection, VtxDataFacade, FVector, Settings->GrowthDirectionAttribute)
		}

		if (Settings->GrowthMaxDistance == EPCGExGrowthValueSource::VtxAttribute)
		{
			PCGEX_GROWTH_GRAB(Context, GrowthMaxDistance, VtxDataFacade, double, Settings->GrowthMaxDistanceAttribute)
		}

		GrowthStop = Settings->bUseGrowthStop ? VtxDataFacade->GetBroadcaster<bool>(Settings->GrowthStopAttribute) : nullptr;
		NoGrowth = Settings->bUseNoGrowth ? VtxDataFacade->GetBroadcaster<bool>(Settings->NoGrowthAttribute) : nullptr;

		if (Settings->bUseOctreeSearch) { Cluster->RebuildOctree(Settings->SeedPicking.PickingMethod); }

		// Prepare growth points

		// Find all growth points
		const int32 SeedCount = TypedContext->SeedsDataFacade->Source->GetNum();
		for (int i = 0; i < SeedCount; i++)
		{
			const FVector SeedPosition = TypedContext->SeedsDataFacade->Source->GetInPoint(i).Transform.GetLocation();
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
				StartGrowthNumBranches = TypedContext->NumBranches->Values[i];
				break;
			case EPCGExGrowthValueSource::VtxAttribute:
				StartGrowthNumBranches = NumBranches->Values[Node.PointIndex];
				break;
			}

			switch (Settings->NumIterations)
			{
			default: ;
			case EPCGExGrowthValueSource::Constant:
				StartNumIterations = Settings->NumIterationsConstant;
				break;
			case EPCGExGrowthValueSource::SeedAttribute:
				StartNumIterations = TypedContext->NumIterations->Values[i];
				break;
			case EPCGExGrowthValueSource::VtxAttribute:
				StartNumIterations = NumIterations->Values[Node.PointIndex];
				break;
			}

			switch (Settings->GrowthMaxDistance)
			{
			default: ;
			case EPCGExGrowthValueSource::Constant:
				StartGrowthMaxDistance = Settings->GrowthMaxDistanceConstant;
				break;
			case EPCGExGrowthValueSource::SeedAttribute:
				StartGrowthMaxDistance = TypedContext->GrowthMaxDistance->Values[i];
				break;
			case EPCGExGrowthValueSource::VtxAttribute:
				StartGrowthMaxDistance = GrowthMaxDistance->Values[Node.PointIndex];
				break;
			}

			switch (Settings->GrowthDirection)
			{
			default: ;
			case EPCGExGrowthValueSource::Constant:
				StartGrowthDirection = Settings->GrowthDirectionConstant;
				break;
			case EPCGExGrowthValueSource::SeedAttribute:
				StartGrowthDirection = TypedContext->GrowthDirection->Values[i];
				break;
			case EPCGExGrowthValueSource::VtxAttribute:
				StartGrowthDirection = GrowthDirection->Values[Node.PointIndex];
				break;
			}

			if (StartGrowthNumBranches <= 0 || StartNumIterations <= 0) { continue; }

			if (Settings->SeedNumBranchesMean == EPCGExMeanMeasure::Relative)
			{
				StartGrowthNumBranches = FMath::Max(1, static_cast<double>(Node.Adjacency.Num()) * StartGrowthNumBranches);
			}

			for (int j = 0; j < StartGrowthNumBranches; j++)
			{
				FGrowth* NewGrowth = new FGrowth(this, Settings, StartNumIterations, Node.NodeIndex, StartGrowthDirection);
				NewGrowth->MaxDistance = StartGrowthMaxDistance;
				NewGrowth->SeedPointIndex = i;

				if (!(NewGrowth->FindNextGrowthNodeIndex() != -1 && NewGrowth->Grow()))
				{
					PCGEX_DELETE(NewGrowth)
					continue;
				}

				Growths.Add(NewGrowth);
				QueuedGrowths.Add(NewGrowth);
			}
		}

		if (IsTrivial()) { Grow(); }
		else { AsyncManagerPtr->Start<FGrowTask>(BatchIndex, nullptr, this); }

		return true;
	}

	void FProcessor::CompleteWork()
	{
		for (FGrowth* Growth : Growths) { Growth->Write(); }
	}

	void FProcessor::Grow()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathfindingGrowPaths)

		if (Settings->GrowthMode == EPCGExGrowthIterationMode::Parallel)
		{
			for (FGrowth* Growth : QueuedGrowths)
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
					FGrowth* Growth = QueuedGrowths[i];

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

	bool FGrowTask::ExecuteTask()
	{
		Processor->Grow();
		return true;
	}
}

#undef PCGEX_GROWTH_GRAB

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
