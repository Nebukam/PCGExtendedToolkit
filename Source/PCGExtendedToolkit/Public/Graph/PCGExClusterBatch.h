// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExEdge.h"
#include "PCGExGraph.h"
#include "PCGExCluster.h"


namespace PCGExClusterBatch
{
	template <typename TBatch>
	class PCGEXTENDEDTOOLKIT_API FStartClusterBatchProcessing : public FPCGExNonAbandonableTask
	{
	public:
		FStartClusterBatchProcessing(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		                             TBatch* InBatchProcessor) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			BatchProcessor(InBatchProcessor)
		{
		}

		TBatch* BatchProcessor = nullptr;

		virtual bool ExecuteTask() override
		{
			if (BatchProcessor->PrepareProcessing()) { BatchProcessor->Process(Manager); }
			return true;
		}
	};

	template <typename TBatch>
	class PCGEXTENDEDTOOLKIT_API FStartClusterBatchCompleteWork : public FPCGExNonAbandonableTask
	{
	public:
		FStartClusterBatchCompleteWork(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		                               TBatch* InBatchData) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			BatchData(InBatchData)
		{
		}

		TBatch* BatchData = nullptr;

		virtual bool ExecuteTask() override
		{
			BatchData->CompleteWork(Manager);
			return true;
		}
	};

	template <typename TSingle>
	class PCGEXTENDEDTOOLKIT_API FStartClusterSingleCompleteWork : public FPCGExNonAbandonableTask
	{
	public:
		FStartClusterSingleCompleteWork(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		                                TSingle* InSingleData) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			SingleData(InSingleData)
		{
		}

		TSingle* SingleData = nullptr;

		virtual bool ExecuteTask() override
		{
			SingleData->CompleteWork(Manager);
			return true;
		}
	};

	template <typename TSingle>
	class PCGEXTENDEDTOOLKIT_API FStartClusterSingleProcessing : public FPCGExNonAbandonableTask
	{
	public:
		FStartClusterSingleProcessing(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		                              TSingle* InSingleData) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			SingleData(InSingleData)
		{
		}

		TSingle* SingleData = nullptr;

		virtual bool ExecuteTask() override
		{
			SingleData->Process(Manager);
			return true;
		}
	};

	template <typename TSingle>
	class PCGEXTENDEDTOOLKIT_API FStartNodeViewProcessing : public FPCGExNonAbandonableTask
	{
	public:
		FStartNodeViewProcessing(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		                         TSingle* InSingleData, int32 InIterations) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			SingleData(InSingleData), Iterations(InIterations)
		{
		}

		TSingle* SingleData = nullptr;
		int32 Iterations = 0;

		virtual bool ExecuteTask() override
		{
			SingleData->ProcessView(TaskIndex, MakeArrayView(SingleData->Cluster->Nodes.GetData() + TaskIndex, Iterations));
			return true;
		}
	};

	template <typename TSingle>
	class PCGEXTENDEDTOOLKIT_API FStartEdgeViewProcessing : public FPCGExNonAbandonableTask
	{
	public:
		FStartEdgeViewProcessing(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		                         TSingle* InSingleData, int32 InIterations) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			SingleData(InSingleData), Iterations(InIterations)
		{
		}

		TSingle* SingleData = nullptr;
		int32 Iterations = 0;

		virtual bool ExecuteTask() override
		{
			SingleData->ProcessView(TaskIndex, MakeArrayView(SingleData->Cluster->Edges.GetData() + TaskIndex, Iterations));
			return true;
		}
	};

	class FClusterProcessingData
	{
	protected:
		virtual bool DefaultVtxFilterResult() const;

		UPCGExNodeStateFactory* VtxFiltersData = nullptr;
		bool DefaultVtxFilterValue = false;

		TArray<bool> VtxFilterCache;
		TArray<bool> EdgeFilterCache;

	public:
		FPCGContext* Context = nullptr;

		PCGExData::FPointIO* Vtx = nullptr;
		PCGExData::FPointIO* Edges = nullptr;
		int32 BatchIndex = -1;

		TMap<int64, int32>* EndpointsLookup = nullptr;
		TArray<int32>* ExpectedAdjacency = nullptr;

		PCGExCluster::FCluster* Cluster = nullptr;

		FClusterProcessingData(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
			Vtx(InVtx), Edges(InEdges)
		{
		}

		virtual ~FClusterProcessingData()
		{
			Vtx = nullptr;
			Edges = nullptr;
		}

		void SetVtxFilterData(UPCGExNodeStateFactory* InVtxFiltersData, const bool DefaultValue)
		{
			VtxFiltersData = InVtxFiltersData;
			DefaultVtxFilterValue = DefaultValue;
		}

		virtual bool Process(FPCGExAsyncManager* AsyncManager)
		{
			Cluster = new PCGExCluster::FCluster();
			if (!Cluster->BuildFrom(*Edges, Vtx->GetIn()->GetPoints(), *EndpointsLookup, ExpectedAdjacency)) { return false; }

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

				if (VtxFiltersHandler->PrepareForTesting(Vtx, VtxIndices)) { for (int32 i : VtxIndices) { VtxFiltersHandler->PrepareSingle((i)); } }
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

			return true;
		}

		void StartParallelLoopForNodes(FPCGExAsyncManager* AsyncManager, int32 PerLoopIterations = 256)
		{
			int32 CurrentCount = 0;
			while (CurrentCount < Cluster->Nodes.Num())
			{
				AsyncManager->Start<FStartNodeViewProcessing<FClusterProcessingData>>(
					CurrentCount, nullptr, this, FMath::Min(Cluster->Nodes.Num() - CurrentCount, PerLoopIterations));
				CurrentCount += PerLoopIterations;
			}
		}

		void StartParallelLoopForEdges(FPCGExAsyncManager* AsyncManager, int32 PerLoopIterations = 256)
		{
			int32 CurrentCount = 0;
			while (CurrentCount < Cluster->Edges.Num())
			{
				AsyncManager->Start<FStartEdgeViewProcessing<FClusterProcessingData>>(
					CurrentCount, nullptr, this, FMath::Min(Cluster->Edges.Num() - CurrentCount, PerLoopIterations));
				CurrentCount += PerLoopIterations;
			}
		}

		virtual void ProcessView(int32 StartIndex, TArrayView<PCGExCluster::FNode> NodeView)
		{
			for (PCGExCluster::FNode& Node : NodeView) { ProcessSingleNode(Node); }
		}

		virtual void ProcessSingleNode(PCGExCluster::FNode& Node)
		{
		}

		virtual void ProcessView(int32 StartIndex, TArrayView<PCGExGraph::FIndexedEdge> EdgeView)
		{
			for (PCGExGraph::FIndexedEdge& Edge : EdgeView) { ProcessSingleEdge(Edge); }
		}

		virtual void ProcessSingleEdge(PCGExGraph::FIndexedEdge& Edge)
		{
		}

		virtual void CompleteWork(FPCGExAsyncManager* AsyncManager)
		{
		}
	};

	template <typename T>
	class FClusterBatchProcessingData
	{
	protected:
		UPCGExNodeStateFactory* VtxFiltersData = nullptr;
		bool bDefaultVtxFilterValue = true;

		UPCGExNodeStateFactory* EdgesFiltersData = nullptr; //TODO
		bool bDefaultEdgeFilterValue = true;                //TODO

	public:
		FPCGContext* Context = nullptr;

		PCGExData::FPointIO* Vtx = nullptr;
		TArray<PCGExData::FPointIO*> Edges;

		TMap<int64, int32> EndpointsLookup;
		TArray<int32> ExpectedAdjacency;

		TArray<FClusterProcessingData*> Processors;

		FClusterBatchProcessingData(FPCGContext* InContext, PCGExData::FPointIO* InVtx, TArrayView<PCGExData::FPointIO*> InEdges):
			Context(InContext), Vtx(InVtx)
		{
			Edges.Append(InEdges);
		}

		virtual ~FClusterBatchProcessingData()
		{
			Context = nullptr;
			Vtx = nullptr;
			Edges.Empty();
			EndpointsLookup.Empty();
			ExpectedAdjacency.Empty();
			PCGEX_DELETE_TARRAY(Processors)
		}

		void SetVtxFilterData(UPCGExNodeStateFactory* InVtxFiltersData, const bool DefaultFilterValue)
		{
			VtxFiltersData = InVtxFiltersData;
			bDefaultVtxFilterValue = DefaultFilterValue;
		}

		virtual bool PrepareProcessing()
		{
			Vtx->CreateInKeys();
			PCGExGraph::BuildEndpointsLookup(*Vtx, EndpointsLookup, ExpectedAdjacency);
			return true;
		}

		void Process(FPCGExAsyncManager* AsyncManager)
		{
			for (PCGExData::FPointIO* IO : Edges)
			{
				IO->CreateInKeys();

				T* NewProcessor = new T(Vtx, IO);
				NewProcessor->Context = Context;
				NewProcessor->EndpointsLookup = &EndpointsLookup;
				NewProcessor->ExpectedAdjacency = &ExpectedAdjacency;

				if (!PrepareSingle(NewProcessor))
				{
					PCGEX_DELETE(NewProcessor)
					continue;
				}

				if (VtxFiltersData) { NewProcessor->SetFilter(VtxFiltersData, bDefaultVtxFilterValue); }

				NewProcessor->BatchIndex = Processors.Add(NewProcessor);
				AsyncManager->Start<FStartClusterSingleProcessing<T>>(IO->IOIndex, IO, NewProcessor);
			}
		}

		virtual bool PrepareSingle(T* ClusterProcessor) = 0;

		virtual void CompleteWork(FPCGExAsyncManager* AsyncManager)
		{
			for (FClusterProcessingData* Processor : Processors) { Processor->CompleteWork(AsyncManager); }
		}
	};


	template <typename T>
	static void ScheduleBatch(FPCGExAsyncManager* Manager, T* Batch)
	{
		Manager->Start<PCGExClusterBatch::FStartClusterBatchProcessing<T>>(-1, nullptr, Batch);
	}

	template <typename T>
	static void CompleteBatches(FPCGExAsyncManager* Manager, TArrayView<T*> Batches)
	{
		for (T* Batch : Batches) { Manager->Start<PCGExClusterBatch::FStartClusterBatchCompleteWork<T>>(-1, nullptr, Batch); }
	}
}
