// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExEdge.h"
#include "PCGExGraph.h"
#include "PCGExCluster.h"
#include "Pathfinding/Heuristics/PCGExHeuristics.h"


namespace PCGExClusterMT
{
	constexpr PCGExMT::AsyncState State_WaitingOnClusterProcessing = __COUNTER__;
	constexpr PCGExMT::AsyncState State_WaitingOnClusterCompletedWork = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ClusterAsyncWorkComplete = __COUNTER__;

#pragma region Tasks

#define PCGEX_CLUSTER_MT_TASK(_NAME, _BODY)\
	template <typename T>\
	class PCGEXTENDEDTOOLKIT_API _NAME final : public FPCGExNonAbandonableTask	{\
	public: _NAME(PCGExData::FPointIO* InPointIO, T* InTarget) : FPCGExNonAbandonableTask(InPointIO),Target(InTarget){} \
		T* Target = nullptr; virtual bool ExecuteTask() override{_BODY return true; }};

#define PCGEX_CLUSTER_MT_TASK_RANGE(_NAME, _BODY)\
	template <typename T>\
	class PCGEXTENDEDTOOLKIT_API _NAME final : public FPCGExNonAbandonableTask	{\
	public: _NAME(PCGExData::FPointIO* InPointIO, T* InTarget, const int32 InIterations) : FPCGExNonAbandonableTask(InPointIO),Target(InTarget), Iterations(InIterations){} \
		T* Target = nullptr; int32 Iterations = 0; virtual bool ExecuteTask() override{_BODY return true; }};

	PCGEX_CLUSTER_MT_TASK(FStartClusterBatchProcessing, { if (Target->PrepareProcessing()) { Target->Process(Manager); } })

	PCGEX_CLUSTER_MT_TASK(FStartClusterBatchCompleteWork, { Target->CompleteWork(); })

	PCGEX_CLUSTER_MT_TASK(FAsyncProcess, { Target->Process(Manager); })

	PCGEX_CLUSTER_MT_TASK(FAsyncCompleteWork, { Target->CompleteWork(); })

	PCGEX_CLUSTER_MT_TASK_RANGE(FAsyncProcessNodeRange, {Target->ProcessView(TaskIndex, MakeArrayView(Target->Cluster->Nodes.GetData() + TaskIndex, Iterations));})

	PCGEX_CLUSTER_MT_TASK_RANGE(FAsyncProcessEdgeRange, {Target->ProcessView(TaskIndex, MakeArrayView(Target->Cluster->Edges.GetData() + TaskIndex, Iterations));})

	PCGEX_CLUSTER_MT_TASK_RANGE(FAsyncProcessRange, {Target->ProcessRange(TaskIndex, Iterations);})

	PCGEX_CLUSTER_MT_TASK_RANGE(FAsyncBatchProcessClosedRange, {Target->ProcessClosedBatchRange(TaskIndex, Iterations);})

#pragma endregion

	class FClusterProcessor
	{
	protected:
		FPCGExAsyncManager* AsyncManagerPtr = nullptr;

	public:
		bool bRequiresHeuristics = false;
		PCGExHeuristics::THeuristicsHandler* HeuristicsHandler = nullptr;

		UPCGExNodeStateFactory* VtxFiltersData = nullptr;
		bool DefaultVtxFilterValue = false;
		bool bIsSmallCluster = false;

		TArray<bool> VtxFilterCache;
		TArray<bool> EdgeFilterCache;

		FPCGContext* Context = nullptr;

		PCGExData::FPointIO* VtxIO = nullptr;
		PCGExData::FPointIO* EdgesIO = nullptr;
		int32 BatchIndex = -1;

		TMap<int64, int32>* EndpointsLookup = nullptr;
		TArray<int32>* ExpectedAdjacency = nullptr;

		PCGExCluster::FCluster* Cluster = nullptr;

		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;

		FClusterProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
			VtxIO(InVtx), EdgesIO(InEdges)
		{
		}

		virtual ~FClusterProcessor()
		{
			PCGEX_DELETE(HeuristicsHandler);

			VtxIO = nullptr;
			EdgesIO = nullptr;
		}

		bool IsTrivial() const { return bIsSmallCluster; }

		void SetVtxFilterData(UPCGExNodeStateFactory* InVtxFiltersData, const bool DefaultValue)
		{
			VtxFiltersData = InVtxFiltersData;
			DefaultVtxFilterValue = DefaultValue;
		}

		virtual bool Process(FPCGExAsyncManager* AsyncManager)
		{
			AsyncManagerPtr = AsyncManager;

			Cluster = new PCGExCluster::FCluster();
			Cluster->PointsIO = VtxIO;
			Cluster->EdgesIO = EdgesIO;

			if (!Cluster->BuildFrom(*EdgesIO, VtxIO->GetIn()->GetPoints(), *EndpointsLookup, ExpectedAdjacency)) { return false; }

			Cluster->RebuildBounds();

#pragma region Vtx filter data

			if (VtxFiltersData)
			{
				VtxFilterCache.SetNumUninitialized(Cluster->Nodes.Num());

				TArray<int32> VtxIndices;
				VtxIndices.SetNumUninitialized(Cluster->Nodes.Num());

				for (int i = 0; i < VtxIndices.Num(); i++)
				{
					VtxIndices[i] = Cluster->Nodes[i].PointIndex;
					VtxFilterCache[i] = DefaultVtxFilterValue;
				}

				PCGExCluster::FNodeStateHandler* VtxFiltersHandler = static_cast<PCGExCluster::FNodeStateHandler*>(VtxFiltersData->CreateFilter());
				VtxFiltersHandler->bCacheResults = false;
				VtxFiltersHandler->CaptureCluster(Context, Cluster);

				if (VtxFiltersHandler->PrepareForTesting(VtxIO, VtxIndices)) { for (int32 i : VtxIndices) { VtxFiltersHandler->PrepareSingle((i)); } }
				for (int i = 0; i < VtxIndices.Num(); i++) { VtxFilterCache[i] = VtxFiltersHandler->Test(VtxIndices[i]); }

				PCGEX_DELETE(VtxFiltersHandler)
				VtxIndices.Empty();
			}
			else
			{
				VtxFilterCache.SetNumUninitialized(Cluster->Nodes.Num());
				for (int i = 0; i < VtxFilterCache.Num(); i++) { VtxFilterCache[i] = DefaultVtxFilterValue; }
			}

#pragma endregion

			if (bRequiresHeuristics)
			{
				HeuristicsHandler = new PCGExHeuristics::THeuristicsHandler(static_cast<FPCGExPointsProcessorContext*>(AsyncManagerPtr->Context));
				HeuristicsHandler->PrepareForCluster(Cluster);
				HeuristicsHandler->CompleteClusterPreparation();
			}

			return true;
		}

		void StartParallelLoopForNodes(const int32 PerLoopIterations = -1)
		{
			if (IsTrivial())
			{
				for (PCGExCluster::FNode& Node : Cluster->Nodes) { ProcessSingleNode(Node); }
				return;
			}

			int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchIteration(PerLoopIterations);
			int32 CurrentCount = 0;
			while (CurrentCount < Cluster->Nodes.Num())
			{
				AsyncManagerPtr->Start<FAsyncProcessNodeRange<FClusterProcessor>>(
					CurrentCount, nullptr, this, FMath::Min(Cluster->Nodes.Num() - CurrentCount, PLI));
				CurrentCount += PLI;
			}
		}

		void StartParallelLoopForEdges(const int32 PerLoopIterations = -1)
		{
			if (IsTrivial())
			{
				for (PCGExGraph::FIndexedEdge& Edge : Cluster->Edges) { ProcessSingleEdge(Edge); }
				return;
			}

			int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchIteration(PerLoopIterations);
			int32 CurrentCount = 0;
			while (CurrentCount < Cluster->Edges.Num())
			{
				AsyncManagerPtr->Start<FAsyncProcessEdgeRange<FClusterProcessor>>(
					CurrentCount, nullptr, this, FMath::Min(Cluster->Edges.Num() - CurrentCount, PLI));
				CurrentCount += PLI;
			}
		}

		void StartParallelLoopForRange(const int32 NumIterations, const int32 PerLoopIterations = -1)
		{
			if (IsTrivial())
			{
				for (int i = 0; i < NumIterations; i++) { ProcessSingleRangeIteration(i); }
				return;
			}

			int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchIteration(PerLoopIterations);
			int32 CurrentCount = 0;
			while (CurrentCount < NumIterations)
			{
				AsyncManagerPtr->Start<FAsyncProcessRange<FClusterProcessor>>(
					CurrentCount, nullptr, this, FMath::Min(NumIterations - CurrentCount, PLI));
				CurrentCount += PLI;
			}
		}

		void ProcessView(int32 StartIndex, const TArrayView<PCGExCluster::FNode> NodeView)
		{
			for (PCGExCluster::FNode& Node : NodeView) { ProcessSingleNode(Node); }
		}

		virtual void ProcessSingleNode(PCGExCluster::FNode& Node)
		{
		}

		void ProcessView(const int32 StartIndex, const TArrayView<PCGExGraph::FIndexedEdge> EdgeView)
		{
			for (PCGExGraph::FIndexedEdge& Edge : EdgeView) { ProcessSingleEdge(Edge); }
		}

		virtual void ProcessSingleEdge(PCGExGraph::FIndexedEdge& Edge)
		{
		}

		void ProcessRange(const int32 StartIndex, const int32 Iterations)
		{
			for (int i = 0; i < Iterations; i++) { ProcessSingleRangeIteration(StartIndex + i); }
		}

		virtual void ProcessSingleRangeIteration(const int32 Iteration)
		{
		}

		virtual void CompleteWork()
		{
		}
	};

	class FClusterProcessorBatchBase
	{
	protected:
		FPCGExAsyncManager* AsyncManagerPtr = nullptr;

		UPCGExNodeStateFactory* VtxFiltersData = nullptr;
		bool bDefaultVtxFilterValue = true;

		UPCGExNodeStateFactory* EdgesFiltersData = nullptr; //TODO
		bool bDefaultEdgeFilterValue = true;                //TODO

		TMap<int64, int32> EndpointsLookup;
		TArray<int32> ExpectedAdjacency;

	public:
		mutable FRWLock BatchLock;

		FPCGContext* Context = nullptr;

		PCGExData::FPointIO* VtxIO = nullptr;
		TArray<PCGExData::FPointIO*> Edges;
		PCGExData::FPointIOCollection* EdgeCollection = nullptr;

		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
		FPCGExGraphBuilderSettings GraphBuilderSettings;

		virtual bool UseGraphBuilder() const { return false; }

		FClusterProcessorBatchBase(FPCGContext* InContext, PCGExData::FPointIO* InVtx, TArrayView<PCGExData::FPointIO*> InEdges):
			Context(InContext), VtxIO(InVtx)
		{
			Edges.Append(InEdges);
		}

		virtual ~FClusterProcessorBatchBase()
		{
			Context = nullptr;
			VtxIO = nullptr;
			Edges.Empty();
			EndpointsLookup.Empty();
			ExpectedAdjacency.Empty();
		}

		virtual bool PrepareProcessing()
		{
			VtxIO->CreateInKeys();
			PCGExGraph::BuildEndpointsLookup(*VtxIO, EndpointsLookup, ExpectedAdjacency);

			if (UseGraphBuilder())
			{
				GraphBuilder = new PCGExGraph::FGraphBuilder(*VtxIO, &GraphBuilderSettings, 6, EdgeCollection);
			}

			return true;
		}

		virtual void Process(FPCGExAsyncManager* AsyncManager)
		{
		}

		virtual void ProcessClosedBatchRange(const int32 StartIndex, const int32 Iterations)
		{
		}

		virtual void CompleteWork()
		{
		}
	};

	template <typename T>
	class TBatch : public FClusterProcessorBatchBase
	{
	public:
		TArray<T*> Processors;
		TArray<T*> ClosedBatchProcessors;

		PCGExMT::AsyncState CurrentState = PCGExMT::State_Setup;

		TBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, const TArrayView<PCGExData::FPointIO*> InEdges):
			FClusterProcessorBatchBase(InContext, InVtx, InEdges)
		{
		}

		virtual ~TBatch() override
		{
			ClosedBatchProcessors.Empty();
			PCGEX_DELETE_TARRAY(Processors)
			PCGEX_DELETE(GraphBuilder)
		}

		virtual bool UseGraphBuilder() const override { return false; }

		void SetVtxFilterData(UPCGExNodeStateFactory* InVtxFiltersData, const bool DefaultFilterValue)
		{
			VtxFiltersData = InVtxFiltersData;
			bDefaultVtxFilterValue = DefaultFilterValue;
		}

		virtual bool PrepareProcessing() override
		{
			return FClusterProcessorBatchBase::PrepareProcessing();
		}

		virtual void Process(FPCGExAsyncManager* AsyncManager) override
		{
			if (VtxIO->GetNum() <= 1) { return; }

			CurrentState = PCGExMT::State_Processing;

			AsyncManagerPtr = AsyncManager;

			for (PCGExData::FPointIO* IO : Edges)
			{
				IO->CreateInKeys();

				T* NewProcessor = new T(VtxIO, IO);
				NewProcessor->Context = Context;
				NewProcessor->EndpointsLookup = &EndpointsLookup;
				NewProcessor->ExpectedAdjacency = &ExpectedAdjacency;

				if (UseGraphBuilder()) { NewProcessor->GraphBuilder = GraphBuilder; }

				if (!PrepareSingle(NewProcessor))
				{
					PCGEX_DELETE(NewProcessor)
					continue;
				}

				if (VtxFiltersData) { NewProcessor->SetVtxFilterData(VtxFiltersData, bDefaultVtxFilterValue); }

				NewProcessor->BatchIndex = Processors.Add(NewProcessor);
				NewProcessor->bIsSmallCluster = IO->GetNum() < GetDefault<UPCGExGlobalSettings>()->SmallClusterSize;

				if (NewProcessor->IsTrivial()) { ClosedBatchProcessors.Add(NewProcessor); }
				else { AsyncManager->Start<FAsyncProcess<T>>(IO->IOIndex, IO, NewProcessor); }
			}

			StartClosedBatchProcessing();
		}

		virtual bool PrepareSingle(T* ClusterProcessor) { return true; };

		virtual void CompleteWork() override
		{
			CurrentState = PCGExMT::State_Completing;
			for (T* Processor : Processors)
			{
				if (Processor->IsTrivial()) { continue; }
				AsyncManagerPtr->Start<FAsyncCompleteWork<T>>(-1, nullptr, Processor);
			}

			StartClosedBatchProcessing();
		}

		virtual void ProcessClosedBatchRange(const int32 StartIndex, const int32 Iterations) override
		{
			if (CurrentState == PCGExMT::State_Processing)
			{
				for (int i = 0; i < Iterations; i++) { ClosedBatchProcessors[StartIndex + i]->Process(AsyncManagerPtr); }
			}
			else if (CurrentState == PCGExMT::State_Completing)
			{
				for (int i = 0; i < Iterations; i++) { ClosedBatchProcessors[StartIndex + i]->CompleteWork(); }
			}
		}

	protected:
		void StartClosedBatchProcessing()
		{
			const int32 NumTrivial = ClosedBatchProcessors.Num();
			if (NumTrivial > 0)
			{
				int32 CurrentCount = 0;
				const int32 PerIterationsNum = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchIteration();
				while (CurrentCount < NumTrivial)
				{
					AsyncManagerPtr->Start<FAsyncBatchProcessClosedRange<FClusterProcessorBatchBase>>(
						CurrentCount, nullptr, this, FMath::Min(NumTrivial - CurrentCount, PerIterationsNum));
					CurrentCount += PerIterationsNum;
				}
			}
		}
	};

	template <typename T>
	class TBatchWithGraphBuilder : public TBatch<T>
	{
	public:
		TBatchWithGraphBuilder(FPCGContext* InContext, PCGExData::FPointIO* InVtx, TArrayView<PCGExData::FPointIO*> InEdges):
			TBatch<T>(InContext, InVtx, InEdges)
		{
		}

		virtual bool UseGraphBuilder() const override { return true; }
	};

	static void ScheduleBatch(FPCGExAsyncManager* Manager, FClusterProcessorBatchBase* Batch)
	{
		Manager->Start<FStartClusterBatchProcessing<FClusterProcessorBatchBase>>(-1, nullptr, Batch);
	}

	static void CompleteBatches(FPCGExAsyncManager* Manager, const TArrayView<FClusterProcessorBatchBase*> Batches)
	{
		for (FClusterProcessorBatchBase* Batch : Batches)
		{
			Manager->Start<FStartClusterBatchCompleteWork<FClusterProcessorBatchBase>>(-1, nullptr, Batch);
		}
	}
}


#undef PCGEX_CLUSTER_MT_TASK
#undef PCGEX_CLUSTER_MT_TASK_RANGE
