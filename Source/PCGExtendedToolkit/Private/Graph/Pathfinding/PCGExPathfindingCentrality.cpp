// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingCentrality.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Graph/PCGExGraph.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"
#include "Graph/Pathfinding/Search/PCGExSearchAStar.h"
#include "Graph/Pathfinding/Search/PCGExSearchOperation.h"
#include "Paths/PCGExPaths.h"
#include "Paths/PCGExShiftPath.h"

#define LOCTEXT_NAMESPACE "PCGExPathfindingCentralityElement"
#define PCGEX_NAMESPACE PathfindingCentrality

#if WITH_EDITOR
void UPCGExPathfindingCentralitySettings::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject) && IsInGameThread())
	{
		if (!SearchAlgorithm) { SearchAlgorithm = NewObject<UPCGExSearchAStar>(this, TEXT("SearchAlgorithm")); }
	}
	Super::PostInitProperties();
}

void UPCGExPathfindingCentralitySettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (SearchAlgorithm) { SearchAlgorithm->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGEX_INITIALIZE_ELEMENT(PathfindingCentrality)
PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(PathfindingCentrality)

TArray<FPCGPinProperties> UPCGExPathfindingCentralitySettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExGraph::SourceHeuristicsLabel, "Heuristics.", Required, FPCGExDataTypeInfoHeuristics::AsId())
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExPathfinding::SourceOverridesSearch)
	return PinProperties;
}

PCGExData::EIOInit UPCGExPathfindingCentralitySettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }
PCGExData::EIOInit UPCGExPathfindingCentralitySettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Forward; }

bool FPCGExPathfindingCentralityElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingCentrality)

	PCGEX_VALIDATE_NAME(Settings->CentralityValueAttributeName)
	
	PCGEX_OPERATION_BIND(SearchAlgorithm, UPCGExSearchInstancedFactory, PCGExPathfinding::SourceOverridesSearch)

	return true;
}

bool FPCGExPathfindingCentralityElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfindingCentralityElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingCentrality)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
			{
				NewBatch->SetWantsHeuristics(true);
				NewBatch->bSkipCompletion = true;
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}


namespace PCGExPathfindingCentrality
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathfindingCentrality::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		SearchOperation = Context->SearchAlgorithm->CreateOperation(); // Create a local copy
		SearchOperation->PrepareForCluster(Cluster.Get());

		bDaisyChainProcessNodes = !Settings->bGreedyQueries;
		if (bDaisyChainProcessNodes) { SearchAllocations = SearchOperation->NewAllocations(); }

		StartParallelLoopForNodes();

		return true;
	}

	void FProcessor::ProcessNodes(const PCGExMT::FScope& Scope)
	{
		const TArray<PCGExCluster::FNode>& Nodes = *Cluster->Nodes.Get();
		TArray<int32>& TraversalTrackerRef = *TraversalTracker;

		PCGExPathfinding::FNodePick SeedPick = PCGExPathfinding::FNodePick(VtxDataFacade->GetInPoint(0));
		SeedPick.Node = &Nodes[0];
		PCGExPathfinding::FNodePick GoalPick = SeedPick;

		TSharedPtr<PCGExPathfinding::FPathQuery> Query = MakeShared<PCGExPathfinding::FPathQuery>(Cluster.ToSharedRef(), SeedPick, GoalPick, -1);
		const TSharedPtr<PCGExPathfinding::FSearchAllocations> Allocations = SearchAllocations ? SearchAllocations : SearchOperation->NewAllocations();
		
		PCGEX_SCOPE_LOOP(Index)
		{
			const int32 NodePointIndex = Nodes[Index].PointIndex;

			SeedPick = PCGExPathfinding::FNodePick(VtxDataFacade->GetInPoint(NodePointIndex));
			SeedPick.Node = &Nodes[Index];
			GoalPick = SeedPick;

			Query->Seed = SeedPick;
			//Query->QueryIndex = Index;

			for (int i = Index+1; i < NumNodes; i++)
			{
				GoalPick = PCGExPathfinding::FNodePick(VtxDataFacade->GetInPoint(Nodes[i].PointIndex));
				GoalPick.Node = &Nodes[i];

				Query->PathNodes.Reset();
				Query->PathEdges.Reset();

				Query->Goal = GoalPick;
				Query->PickResolution = PCGExPathfinding::EQueryPickResolution::Success;
				Query->FindPath(SearchOperation, Allocations, HeuristicsHandler, nullptr);

				if (!Query->IsQuerySuccessful()) { continue; }

				FPlatformAtomics::InterlockedIncrement(&TraversalTrackerRef[NodePointIndex]);
			}
			UE_LOG(LogTemp, Log, TEXT("Traversal... %d"), Index);
		}
	}

	void FProcessor::OnNodesProcessingComplete()
	{
		const TArray<PCGExCluster::FNode>& Nodes = *Cluster->Nodes.Get();
		TArray<int32>& TraversalTrackerRef = *TraversalTracker;

		if (Settings->Units == EPCGExMeanMeasure::Discrete)
		{
			TSharedPtr<PCGExData::TBuffer<int32>> Buffer = VtxDataFacade->GetWritable<int32>(Settings->CentralityValueAttributeName, 0, true, PCGExData::EBufferInit::New);
			for (int i = 0; i < NumNodes; i++)
			{
				const int32 NodePtIndex = Nodes[i].PointIndex;
				Buffer->SetValue(NodePtIndex, TraversalTrackerRef[Nodes[i].PointIndex]);
			}
		}
		else
		{
			TSharedPtr<PCGExData::TBuffer<double>> Buffer = VtxDataFacade->GetWritable<double>(Settings->CentralityValueAttributeName, Settings->bOutputOneMinus ? 1 : 0, true, PCGExData::EBufferInit::New);
			double Max = 0;
			for (int i = 0; i < NumNodes; i++) { Max = FMath::Max(Max, TraversalTrackerRef[Nodes[i].PointIndex]); }

			if (Settings->bOutputOneMinus)
			{
				for (int i = 0; i < NumNodes; i++)
				{
					const int32 NodePtIndex = Nodes[i].PointIndex;
					Buffer->SetValue(NodePtIndex, 1 - (TraversalTrackerRef[Nodes[i].PointIndex] / Max));
				}
			}
			else
			{
				for (int i = 0; i < NumNodes; i++)
				{
					const int32 NodePtIndex = Nodes[i].PointIndex;
					Buffer->SetValue(NodePtIndex, TraversalTrackerRef[Nodes[i].PointIndex] / Max);
				}
			}
		}
	}

	FBatch::FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
		: TBatch(InContext, InVtx, InEdges)
	{
		TraversalTracker.Init(0, InVtx->GetNum());
	}

	bool FBatch::PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor)
	{
		if (!TBatch<FProcessor>::PrepareSingle(InProcessor)) { return false; }
		TSharedPtr<FProcessor> P = StaticCastSharedPtr<FProcessor>(InProcessor);
		P->TraversalTracker = &TraversalTracker;
		return true;
	}

	void FBatch::Write()
	{
		VtxDataFacade->WriteFastest(AsyncManager);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
