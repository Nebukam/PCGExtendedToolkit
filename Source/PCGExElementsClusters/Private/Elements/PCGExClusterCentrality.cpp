// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExClusterCentrality.h"

#include "PCGExHeuristicsHandler.h"
#include "PCGParamData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Clusters/PCGExCluster.h"
#include "Containers/PCGExScopedContainers.h"
#include "Core/PCGExHeuristicsFactoryProvider.h"
#include "Core/PCGExPointFilter.h"
#include "Utils/PCGExScoredQueue.h"

#define LOCTEXT_NAMESPACE "PCGExClusterCentralityElement"
#define PCGEX_NAMESPACE ClusterCentrality

PCGEX_INITIALIZE_ELEMENT(ClusterCentrality)
PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(ClusterCentrality)

#if WITH_EDITOR
void UPCGExClusterCentralitySettings::ApplyDeprecation(UPCGNode* InOutNode)
{
	PCGEX_UPDATE_TO_DATA_VERSION(1, 73, 0)
	{
		RandomDownsampling.ApplyDeprecation();
	}

	Super::ApplyDeprecation(InOutNode);
}
#endif

bool UPCGExClusterCentralitySettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	if (InPin->Properties.Label == PCGExClusters::Labels::SourceVtxFiltersLabel) { return DownsamplingMode == EPCGExCentralityDownsampling::Filters; }
	return Super::IsPinUsedByNodeExecution(InPin);
}

TArray<FPCGPinProperties> UPCGExClusterCentralitySettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExHeuristics::Labels::SourceHeuristicsLabel, "Heuristics.", Required, FPCGExDataTypeInfoHeuristics::AsId())

	if (DownsamplingMode == EPCGExCentralityDownsampling::Filters) { PCGEX_PIN_FILTERS(PCGExClusters::Labels::SourceVtxFiltersLabel, "Vtx filters.", Required) }
	else { PCGEX_PIN_FILTERS(PCGExClusters::Labels::SourceVtxFiltersLabel, "Vtx filters.", Advanced) }

	return PinProperties;
}

PCGExData::EIOInit UPCGExClusterCentralitySettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }
PCGExData::EIOInit UPCGExClusterCentralitySettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Forward; }

bool FPCGExClusterCentralityElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ClusterCentrality)

	PCGEX_VALIDATE_NAME(Settings->CentralityValueAttributeName)

	if (Settings->DownsamplingMode == EPCGExCentralityDownsampling::Filters)
	{
		if (!GetInputFactories(Context, PCGExClusters::Labels::SourceVtxFiltersLabel, Context->VtxFilterFactories, PCGExFactories::ClusterNodeFilters))
		{
			return false;
		}
	}

	return true;
}

bool FPCGExClusterCentralityElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExClusterCentralityElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ClusterCentrality)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters([](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; }, [&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
		{
			NewBatch->SetWantsHeuristics(true);
			NewBatch->bSkipCompletion = true;
			NewBatch->bRequiresWriteStep = true;
			if (Settings->DownsamplingMode == EPCGExCentralityDownsampling::Filters)
			{
				NewBatch->VtxFilterFactories = &Context->VtxFilterFactories;
			}
		}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExClusterCentrality
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExClusterCentrality::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

		Betweenness.Init(0.0, NumNodes);

		bDownsample = Settings->DownsamplingMode != EPCGExCentralityDownsampling::None;
		if (bDownsample)
		{
			if (Settings->DownsamplingMode == EPCGExCentralityDownsampling::Ratio)
			{
				Settings->RandomDownsampling.GetPicks(Context, VtxDataFacade->GetIn(), NumNodes, RandomSamples);
			}
			else
			{
				PCGExArrayHelpers::ArrayOfIndices(RandomSamples, NumNodes);
				bVtxComplete = false;
				StartParallelLoopForNodes();
			}
		}

		DirectedEdgeScores.SetNum(NumEdges * 2);
		StartParallelLoopForEdges();

		return true;
	}

	void FProcessor::ProcessEdges(const PCGExMT::FScope& Scope)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			const PCGExClusters::FEdge& Edge = *Cluster->GetEdge(Index);
			const PCGExClusters::FNode& Start = *Cluster->GetEdgeStart(Edge);
			const PCGExClusters::FNode& End = *Cluster->GetEdgeStart(Edge);

			DirectedEdgeScores[Index] = HeuristicsHandler->GetEdgeScore(Start, End, Edge, *Cluster->GetNode(Index), *Cluster->GetNode(Index), nullptr, nullptr);

			DirectedEdgeScores[NumEdges + Index] = HeuristicsHandler->GetEdgeScore(End, Start, Edge, *Cluster->GetNode(Index), *Cluster->GetNode(Index), nullptr, nullptr);
		}
	}

	void FProcessor::OnEdgesProcessingComplete()
	{
		{
			FWriteScopeLock WriteScopeLock(CompletionLock);
			bEdgeComplete = true;
		}

		TryStartCompute();
	}

	void FProcessor::ProcessNodes(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExClusterCentrality::ProcessNodes);
		FilterVtxScope(Scope);
	}

	void FProcessor::OnNodesProcessingComplete()
	{
		{
			FWriteScopeLock WriteScopeLock(CompletionLock);
			bVtxComplete = true;
		}

		TryStartCompute();
	}

	void FProcessor::TryStartCompute()
	{
		{
			FReadScopeLock ReadScopeLock(CompletionLock);
			if (!bVtxComplete || !bEdgeComplete) { return; }
		}

		if (Settings->DownsamplingMode == EPCGExCentralityDownsampling::Filters)
		{
			int32 WriteIndex = 0;
			for (int i = 0; i < NumNodes; i++)
			{
				if (IsNodePassingFilters(*(Cluster->Nodes->GetData() + i)))
				{
					RandomSamples[WriteIndex++] = i;
				}
			}

			RandomSamples.SetNum(WriteIndex);
		}

		if (bDownsample && RandomSamples.IsEmpty()) { RandomSamples.Add(0); }

		StartParallelLoopForRange(bDownsample ? RandomSamples.Num() : NumNodes, 128);
	}

	void FProcessor::PrepareLoopScopesForRanges(const TArray<PCGExMT::FScope>& Loops)
	{
		ScopedBetweenness = MakeShared<PCGExMT::TScopedArray<double>>(Loops);
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExClusterCentrality::ProcessNodes);

		TArray<double>& LocalBetweenness = ScopedBetweenness->Get_Ref(Scope);
		LocalBetweenness.Init(0.0, NumNodes);

		TArray<double> Score;
		Score.Init(DBL_MAX, NumNodes);

		TArray<double> Sigma;
		Sigma.Init(0.0, NumNodes);

		TArray<double> Delta;
		Delta.Init(0.0, NumNodes);

		TArray<NodePred> Pred;
		Pred.SetNum(NumNodes);

		TArray<int32> Stack;
		Stack.Reserve(NumNodes);

		TSharedPtr<PCGEx::FScoredQueue> Queue = MakeShared<PCGEx::FScoredQueue>(NumNodes);

		if (bDownsample)
		{
			const double Ratio = static_cast<double>(NumNodes) / static_cast<double>(RandomSamples.Num());

			PCGEX_SCOPE_LOOP(Index)
			{
				ProcessSingleNode(RandomSamples[Index], LocalBetweenness, Score, Sigma, Delta, Pred, Stack, Queue);
				for (double& B : LocalBetweenness) { B *= Ratio; }
			}
		}
		else
		{
			PCGEX_SCOPE_LOOP(Index)
			{
				ProcessSingleNode(Index, LocalBetweenness, Score, Sigma, Delta, Pred, Stack, Queue);
			}
		}
	}

	void FProcessor::ProcessSingleNode(const int32 Index, TArray<double>& LocalBetweenness, TArray<double>& Score, TArray<double>& Sigma, TArray<double>& Delta, TArray<NodePred>& Pred, TArray<int32>& Stack, const TSharedPtr<PCGEx::FScoredQueue>& Queue)
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
			const PCGExClusters::FNode& Current = *Cluster->GetNode(CurrentNode);

			for (const PCGExGraphs::FLink Lk : Current.Links)
			{
				const int32 Neighbor = Lk.Node;
				const int32 EdgeIndex = Lk.Edge;
				const PCGExGraphs::FEdge& Edge = *Cluster->GetEdge(EdgeIndex);

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
			for (const int32 V : Pred[W]) { Delta[V] += (Sigma[V] / Sigma[W]) * (1.0 + Delta[W]); }
			if (W != Index) { LocalBetweenness[W] += Delta[W]; }
		}
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		ScopedBetweenness->ForEach([&](TArray<double>& ScopedArray)
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

		const TArray<PCGExClusters::FNode>& Nodes = *Cluster->Nodes.Get();
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
		VtxDataFacade->WriteFastest(TaskManager);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
