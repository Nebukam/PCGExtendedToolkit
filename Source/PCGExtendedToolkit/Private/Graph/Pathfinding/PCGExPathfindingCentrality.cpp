// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingCentrality.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Graph/PCGExGraph.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"
#include "Graph/Pathfinding/Search/PCGExScoredQueue.h"
#include "Graph/Pathfinding/Search/PCGExSearchAStar.h"
#include "Graph/Pathfinding/Search/PCGExSearchOperation.h"
#include "Paths/PCGExPaths.h"
#include "Paths/PCGExShiftPath.h"

#define LOCTEXT_NAMESPACE "PCGExPathfindingCentralityElement"
#define PCGEX_NAMESPACE PathfindingCentrality

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

		Betweenness.Init(0.0, NumNodes);

		StartParallelLoopForEdges();

		return true;
	}

	void FProcessor::PrepareLoopScopesForEdges(const TArray<PCGExMT::FScope>& Loops)
	{
		DirectedEdgeScores.SetNum(NumEdges * 2);
	}

	void FProcessor::ProcessEdges(const PCGExMT::FScope& Scope)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			const PCGExCluster::FEdge& Edge = *Cluster->GetEdge(Index);
			const PCGExCluster::FNode& Start = *Cluster->GetEdgeStart(Edge);
			const PCGExCluster::FNode& End = *Cluster->GetEdgeStart(Edge);

			DirectedEdgeScores[Index] = HeuristicsHandler->GetEdgeScore(
				Start, End, Edge,
				*Cluster->GetNode(Index), *Cluster->GetNode(Index), nullptr, nullptr);

			DirectedEdgeScores[NumEdges + Index] = HeuristicsHandler->GetEdgeScore(
				End, Start, Edge,
				*Cluster->GetNode(Index), *Cluster->GetNode(Index), nullptr, nullptr);
		}
	}

	void FProcessor::OnEdgesProcessingComplete()
	{
		StartParallelLoopForNodes(256);
	}

	void FProcessor::PrepareLoopScopesForNodes(const TArray<PCGExMT::FScope>& Loops)
	{
		ScopedBetweenness = MakeShared<PCGExMT::TScopedArray<double>>(Loops);
	}

	void FProcessor::ProcessNodes(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathfindingCentrality::ProcessNodes);

		TArray<double>& LocalBetweenness = ScopedBetweenness->Get_Ref(Scope);
		LocalBetweenness.Init(0.0, NumNodes);

		TArray<double> Score;
		Score.Init(DBL_MAX, NumNodes);

		TArray<double> Sigma;
		Sigma.Init(0.0, NumNodes);

		TArray<double> Delta;
		Delta.Init(0.0, NumNodes);

		TArray<TArray<int32>> Pred;
		Pred.SetNum(NumNodes);
		for (TArray<int32>& P : Pred) { P.Reserve(8); }

		TArray<int32> Stack;
		Stack.Reserve(NumNodes);

		TSharedPtr<PCGExSearch::FScoredQueue> Queue = MakeShared<PCGExSearch::FScoredQueue>(NumNodes);

		PCGEX_SCOPE_LOOP(Index)
		{
			for (int i = 0; i < NumNodes; i++)
			{
				Score[i] = DBL_MAX;
				Sigma[i] = 0;
				Delta[i] = 0;
			}

			Stack.Reset();

			Score[Index] = 0.0;
			Sigma[Index] = 1.0;

			Queue->Reset();
			Queue->Enqueue(Index, 0.0);

			int32 CurrentNode;
			double CurrentScore;

			while (Queue->Dequeue(CurrentNode, CurrentScore))
			{
				Stack.Add(CurrentNode);
				const PCGExCluster::FNode& Current = *Cluster->GetNode(CurrentNode);

				for (const PCGExGraph::FLink Lk : Current.Links)
				{
					const int32 Neighbor = Lk.Node;
					const int32 EdgeIndex = Lk.Edge;
					const PCGExGraph::FEdge& Edge = *Cluster->GetEdge(EdgeIndex);

					const double EdgeCost = Edge.Start == Current.PointIndex ? DirectedEdgeScores[Edge.PointIndex] : DirectedEdgeScores[NumEdges + Edge.PointIndex];
					const double NewDist = Score[CurrentNode] + EdgeCost;

					if (NewDist < Score[Neighbor])
					{
						Score[Neighbor] = NewDist;
						Queue->Enqueue(Neighbor, NewDist);
						Pred[Neighbor].Reset();
						Pred[Neighbor].Add(CurrentNode);
						Sigma[Neighbor] = Sigma[CurrentNode];
					}
					else if (FMath::IsNearlyEqual(NewDist, Score[Neighbor]))
					{
						Pred[Neighbor].Add(CurrentNode);
						Sigma[Neighbor] += Sigma[CurrentNode];
					}
				}
			}

			// Accumulate dependencies
			for (int32 i = Stack.Num() - 1; i >= 0; --i)
			{
				const int32 W = Stack[i];
				for (int32 V : Pred[W]) { Delta[V] += (Sigma[V] / Sigma[W]) * (1.0 + Delta[W]); }
				if (W != Index) { LocalBetweenness[W] += Delta[W]; }
			}
		}
	}

	void FProcessor::OnNodesProcessingComplete()
	{
		ScopedBetweenness->ForEach(
			[&](TArray<double>& ScopedArray)
			{
				for (int i = 0; i < NumNodes; i++) { Betweenness[i] += ScopedArray[i]; }
				ScopedArray.Empty();
			});

		ScopedBetweenness.Reset();

		double Max = 0;
		for (double& C : Betweenness)
		{
			// Normalize for undirected graphs
			C *= 0.5;
			Max = FMath::Max(Max, C);
		}

		const TArray<PCGExCluster::FNode>& Nodes = *Cluster->Nodes.Get();
		TSharedPtr<PCGExData::TBuffer<double>> Buffer = VtxDataFacade->GetWritable<double>(Settings->CentralityValueAttributeName, Settings->bOutputOneMinus ? 1 : 0, true, PCGExData::EBufferInit::New);

		for (int i = 0; i < NumNodes; i++) { Max = FMath::Max(Max, Betweenness[i]); }

		if (Settings->bNormalize)
		{
			if (Settings->bOutputOneMinus)
			{
				for (int i = 0; i < NumNodes; i++) { Buffer->SetValue(Nodes[i].PointIndex, 1 - (Betweenness[i] / Max)); }
			}
			else
			{
				for (int i = 0; i < NumNodes; i++) { Buffer->SetValue(Nodes[i].PointIndex, Betweenness[i] / Max); }
			}
		}
		else
		{
			for (int i = 0; i < NumNodes; i++) { Buffer->SetValue(Nodes[i].PointIndex, Betweenness[i]); }
		}
	}

	FBatch::FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
		: TBatch(InContext, InVtx, InEdges)
	{
	}

	void FBatch::Write()
	{
		VtxDataFacade->WriteFastest(AsyncManager);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
