// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExEdge.h"
#include "PCGExGraph.h"
#include "PCGExCluster.h"
#include "Data/PCGExClusterData.h"
#include "Data/PCGExDataCaching.h"
#include "Filters/PCGExClusterFilter.h"
#include "Pathfinding/Heuristics/PCGExHeuristics.h"


namespace PCGExClusterMT
{
	PCGEX_ASYNC_STATE(MTState_ClusterProcessing)
	PCGEX_ASYNC_STATE(MTState_ClusterCompletingWork)
	PCGEX_ASYNC_STATE(MTState_ClusterWriting)

#pragma region Tasks

#define PCGEX_CLUSTER_MT_TASK(_NAME, _BODY)\
	template <typename T>\
	class PCGEXTENDEDTOOLKIT_API _NAME final : public PCGExMT::FPCGExTask	{\
	public: _NAME(PCGExData::FPointIO* InPointIO, T* InTarget) : PCGExMT::FPCGExTask(InPointIO),Target(InTarget){} \
		T* Target = nullptr; virtual bool ExecuteTask() override{_BODY return true; }};

#define PCGEX_CLUSTER_MT_TASK_RANGE(_NAME, _BODY)\
	template <typename T>\
	class PCGEXTENDEDTOOLKIT_API _NAME final : public PCGExMT::FPCGExTask	{\
	public: _NAME(PCGExData::FPointIO* InPointIO, T* InTarget, const uint64 InIterations) : PCGExMT::FPCGExTask(InPointIO),Target(InTarget), Iterations(InIterations){} \
		T* Target = nullptr; uint64 Iterations = 0; virtual bool ExecuteTask() override{_BODY return true; }};

#define PCGEX_CLUSTER_MT_TASK_Scope(_NAME, _BODY)\
	template <typename T>\
	class PCGEXTENDEDTOOLKIT_API _NAME final : public PCGExMT::FPCGExTask	{\
	public: _NAME(PCGExData::FPointIO* InPointIO, T* InTarget, const TArray<uint64>& InScopes) : PCGExMT::FPCGExTask(InPointIO),Target(InTarget), Scopes(InScopes){} \
	T* Target = nullptr; TArray<uint64> Scopes; virtual bool ExecuteTask() override{_BODY return true; }};

	PCGEX_CLUSTER_MT_TASK(FStartClusterBatchProcessing, { if (Target->PrepareProcessing()) { Target->Process(Manager); } })

	PCGEX_CLUSTER_MT_TASK(FStartClusterBatchCompleteWork, { Target->CompleteWork(); })

	PCGEX_CLUSTER_MT_TASK(FAsyncProcess, { Target->Process(Manager); })

	PCGEX_CLUSTER_MT_TASK(FAsyncProcessWithUpdate, { Target->bIsProcessorValid = Target->Process(Manager); })

	PCGEX_CLUSTER_MT_TASK(FAsyncCompleteWork, { Target->CompleteWork(); })

	PCGEX_CLUSTER_MT_TASK_RANGE(FAsyncProcessNodeRange, {Target->ProcessView(TaskIndex, MakeArrayView(Target->Cluster->Nodes->GetData() + TaskIndex, Iterations));})

	PCGEX_CLUSTER_MT_TASK_RANGE(FAsyncProcessEdgeRange, {Target->ProcessView(TaskIndex, MakeArrayView(Target->Cluster->Edges->GetData() + TaskIndex, Iterations));})

	PCGEX_CLUSTER_MT_TASK_RANGE(FAsyncProcessRange, {Target->ProcessRange(TaskIndex, Iterations);})

	PCGEX_CLUSTER_MT_TASK_RANGE(FAsyncProcessScope, {Target->ProcessScope(Iterations);})

	PCGEX_CLUSTER_MT_TASK_Scope(FAsyncProcessScopeList, {for(const uint64 Scope: Scopes ){ Target->ProcessScope(Scope);}})

	PCGEX_CLUSTER_MT_TASK_RANGE(FAsyncBatchProcessClosedRange, {Target->ProcessClosedBatchRange(TaskIndex, Iterations);})

#pragma endregion

	class FClusterProcessor
	{
		friend class FClusterProcessorBatchBase;

	protected:
		PCGExMT::FTaskManager* AsyncManagerPtr = nullptr;
		bool bBuildCluster = true;
		bool bRequiresHeuristics = false;
		bool bCacheVtxPointIndices = false;
		bool bDeleteCluster = false;

		int32 NumNodes = 0;

		virtual PCGExCluster::FCluster* HandleCachedCluster(const PCGExCluster::FCluster* InClusterRef)
		{
			return new PCGExCluster::FCluster(InClusterRef, VtxIO, EdgesIO, false, false, false);
		}

		void ForwardCluster(bool bAsOwner)
		{
			if (UPCGExClusterEdgesData* EdgesData = Cast<UPCGExClusterEdgesData>(VtxIO->GetOut()))
			{
				EdgesData->SetBoundCluster(Cluster, bAsOwner);
				bDeleteCluster = false;
			}
		}

	public:
		PCGExDataCaching::FPool* VtxDataCache = nullptr;
		PCGExDataCaching::FPool* EdgeDataCache = nullptr;

		bool bIsProcessorValid = false;

		PCGExHeuristics::THeuristicsHandler* HeuristicsHandler = nullptr;

		const TArray<UPCGExFilterFactoryBase*>* VtxFilterFactories = nullptr;
		bool DefaultVtxFilterValue = false;
		bool bIsSmallCluster = false;
		bool bIsOneToOne = false;

		TArray<bool> VtxFilterCache;
		TArray<bool> EdgeFilterCache;
		const TArray<int32>* VtxPointIndicesCache = nullptr;

		FPCGContext* Context = nullptr;

		PCGExData::FPointIO* VtxIO = nullptr;
		PCGExData::FPointIO* EdgesIO = nullptr;
		int32 BatchIndex = -1;

		TMap<uint32, int32>* EndpointsLookup = nullptr;
		TArray<int32>* ExpectedAdjacency = nullptr;

		PCGExCluster::FCluster* Cluster = nullptr;

		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;

		FClusterProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
			VtxIO(InVtx), EdgesIO(InEdges)
		{
			PCGEX_LOG_CTR(FClusterProcessor)
			EdgeDataCache = new PCGExDataCaching::FPool(InEdges);
		}

		virtual ~FClusterProcessor()
		{
			PCGEX_LOG_DTR(FClusterProcessor)

			PCGEX_DELETE(HeuristicsHandler);
			if (bDeleteCluster) { PCGEX_DELETE(Cluster); } // Safely delete the cluster

			VtxIO = nullptr;
			EdgesIO = nullptr;

			VtxFilterCache.Empty();
			EdgeFilterCache.Empty();

			PCGEX_DELETE(EdgeDataCache)
		}

		template <typename T>
		T* GetContext() { return static_cast<T*>(Context); }

		bool IsTrivial() const { return bIsSmallCluster; }

		void SetVtxFilterFactories(const TArray<UPCGExFilterFactoryBase*>* InFactories)
		{
			VtxFilterFactories = InFactories;
		}

		void SetRequiresHeuristics(const bool bRequired = false) { bRequiresHeuristics = bRequired; }

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager)
		{
			AsyncManagerPtr = AsyncManager;

			if (!bBuildCluster) { return true; }

			if (const PCGExCluster::FCluster* CachedCluster = PCGExClusterData::TryGetCachedCluster(VtxIO, EdgesIO))
			{
				Cluster = HandleCachedCluster(CachedCluster);
			}

			if (!Cluster)
			{
				Cluster = new PCGExCluster::FCluster();
				Cluster->bIsOneToOne = bIsOneToOne;
				Cluster->VtxIO = VtxIO;
				Cluster->EdgesIO = EdgesIO;

				if (!Cluster->BuildFrom(EdgesIO, VtxIO->GetIn()->GetPoints(), *EndpointsLookup, ExpectedAdjacency))
				{
					PCGEX_DELETE(Cluster)
					return false;
				}

				Cluster->RebuildBounds();
			}

			NumNodes = Cluster->Nodes->Num();

#pragma region Vtx filter data

			if (bCacheVtxPointIndices || VtxFilterFactories)
			{
				VtxPointIndicesCache = Cluster->GetVtxPointIndicesPtr();
			}

			if (VtxFilterFactories)
			{
				VtxFilterCache.SetNumUninitialized(VtxPointIndicesCache->Num());

				for (int i = 0; i < VtxPointIndicesCache->Num(); i++) { VtxFilterCache[i] = DefaultVtxFilterValue; }

				PCGExClusterFilter::TManager* FilterManager = new PCGExClusterFilter::TManager(Cluster, VtxDataCache, EdgeDataCache);
				FilterManager->Init(Context, *VtxFilterFactories);

				const TArray<PCGExCluster::FNode>& NodesRef = *Cluster->Nodes;
				for (const PCGExCluster::FNode& Node : NodesRef) { VtxFilterCache[Node.NodeIndex] = FilterManager->Test(Node); }

				PCGEX_DELETE(FilterManager)
			}
			else
			{
				VtxFilterCache.SetNumUninitialized(Cluster->Nodes->Num());
				for (int i = 0; i < VtxFilterCache.Num(); i++) { VtxFilterCache[i] = DefaultVtxFilterValue; }
			}

#pragma endregion

			if (bRequiresHeuristics)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FClusterProcessor::Heuristics);
				HeuristicsHandler = new PCGExHeuristics::THeuristicsHandler(Context, VtxDataCache, EdgeDataCache);
				HeuristicsHandler->PrepareForCluster(Cluster);
				HeuristicsHandler->CompleteClusterPreparation();
			}

			return true;
		}

#pragma region Parallel loops

		void StartParallelLoopForNodes(const int32 PerLoopIterations = -1)
		{
			if (IsTrivial())
			{
				TArray<PCGExCluster::FNode>& NodesRef = (*Cluster->Nodes);
				for (int i = 0; i < NodesRef.Num(); i++) { ProcessSingleNode(i, NodesRef[i]); }
				return;
			}

			int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchIteration(PerLoopIterations);
			int32 CurrentCount = 0;
			while (CurrentCount < Cluster->Nodes->Num())
			{
				AsyncManagerPtr->Start<FAsyncProcessNodeRange<FClusterProcessor>>(
					CurrentCount, nullptr, this, FMath::Min(Cluster->Nodes->Num() - CurrentCount, PLI));
				CurrentCount += PLI;
			}
		}

		void StartParallelLoopForEdges(const int32 PerLoopIterations = -1)
		{
			if (IsTrivial())
			{
				for (PCGExGraph::FIndexedEdge& Edge : (*Cluster->Edges)) { ProcessSingleEdge(Edge); }
				return;
			}

			int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchIteration(PerLoopIterations);
			int32 CurrentCount = 0;
			while (CurrentCount < Cluster->Edges->Num())
			{
				AsyncManagerPtr->Start<FAsyncProcessEdgeRange<FClusterProcessor>>(
					CurrentCount, nullptr, this, FMath::Min(Cluster->Edges->Num() - CurrentCount, PLI));
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

		virtual void ProcessView(int32 StartIndex, const TArrayView<PCGExCluster::FNode> NodeView)
		{
			for (int i = 0; i < NodeView.Num(); i++) { ProcessSingleNode(i, NodeView[i]); }
		}

		virtual void ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node)
		{
		}

		virtual void ProcessView(const int32 StartIndex, const TArrayView<PCGExGraph::FIndexedEdge> EdgeView)
		{
			for (PCGExGraph::FIndexedEdge& Edge : EdgeView) { ProcessSingleEdge(Edge); }
		}

		virtual void ProcessSingleEdge(PCGExGraph::FIndexedEdge& Edge)
		{
		}

		virtual void ProcessRange(const int32 StartIndex, const int32 Iterations)
		{
			for (int i = 0; i < Iterations; i++) { ProcessSingleRangeIteration(StartIndex + i); }
		}

		virtual void ProcessSingleRangeIteration(const int32 Iteration)
		{
		}

		void StartParallelLoopForScopes(const TArrayView<const uint64>& InScopes)
		{
			if (IsTrivial())
			{
				for (const uint64 Scope : InScopes) { ProcessScope(Scope); }
				return;
			}

			int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchIteration(-1);
			int32 CurrentCount = 0;

			TArray<uint64> Scopes;
			Scopes.Reserve(PLI);

			int32 CurrentSum = 0;

			for (uint64 Scope : InScopes)
			{
				uint32 Count = PCGEx::H64B(Scope);
				CurrentSum += Count;
				if (CurrentSum > PLI)
				{
					if (Scopes.IsEmpty())
					{
						AsyncManagerPtr->Start<FAsyncProcessScope<FClusterProcessor>>(-1, nullptr, this, Scope);
					}
					else
					{
						AsyncManagerPtr->Start<FAsyncProcessScopeList<FClusterProcessor>>(-1, nullptr, this, Scopes);
					}
					Scopes.Reset();
					CurrentSum = 0;
					continue;
				}

				Scopes.Add(Scope);
			}
			if (!Scopes.IsEmpty())
			{
				if (Scopes.Num() == 1)
				{
					AsyncManagerPtr->Start<FAsyncProcessScope<FClusterProcessor>>(-1, nullptr, this, Scopes[0]);
				}
				else
				{
					AsyncManagerPtr->Start<FAsyncProcessScopeList<FClusterProcessor>>(-1, nullptr, this, Scopes);
				}
			}
		}

		virtual void ProcessScope(const uint64 Scope)
		{
		}

#pragma endregion

		virtual void CompleteWork()
		{
		}

		virtual void Write()
		{
		}
	};

	class FClusterProcessorBatchBase
	{
	protected:
		PCGExMT::FTaskManager* AsyncManagerPtr = nullptr;

		const TArray<UPCGExFilterFactoryBase*>* VtxFilterFactories = nullptr;
		const TArray<UPCGExFilterFactoryBase*>* EdgeFilterFactories = nullptr; //TODO

		TMap<uint32, int32> EndpointsLookup;
		TArray<int32> ExpectedAdjacency;

		bool bRequiresHeuristics = false;
		bool bRequiresGraphBuilder = false;

	public:
		PCGExDataCaching::FPool* VtxDataCache = nullptr;

		bool bRequiresWriteStep = false;

		mutable FRWLock BatchLock;

		FPCGContext* Context = nullptr;

		PCGExData::FPointIO* VtxIO = nullptr;
		TArray<PCGExData::FPointIO*> Edges;
		PCGExData::FPointIOCollection* EdgeCollection = nullptr;

		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
		FPCGExGraphBuilderSettings GraphBuilderSettings;

		TArray<PCGExCluster::FCluster*> ValidClusters;

		void SetVtxFilterFactories(const TArray<UPCGExFilterFactoryBase*>* InVtxFilterFactories)
		{
			VtxFilterFactories = InVtxFilterFactories;
		}

		bool RequiresGraphBuilder() const { return bRequiresGraphBuilder; }
		bool RequiresHeuristics() const { return bRequiresHeuristics; }
		virtual void SetRequiresHeuristics(const bool bRequired = false) { bRequiresHeuristics = bRequired; }

		bool bInlineProcessing = false;
		bool bInlineCompletion = false;

		FClusterProcessorBatchBase(FPCGContext* InContext, PCGExData::FPointIO* InVtx, TArrayView<PCGExData::FPointIO*> InEdges):
			Context(InContext), VtxIO(InVtx)
		{
			Edges.Append(InEdges);
			VtxDataCache = new PCGExDataCaching::FPool(InVtx);
		}

		virtual ~FClusterProcessorBatchBase()
		{
			Context = nullptr;
			VtxIO = nullptr;

			Edges.Empty();
			EndpointsLookup.Empty();
			ExpectedAdjacency.Empty();

			PCGEX_DELETE(GraphBuilder)
			PCGEX_DELETE(VtxDataCache)
		}

		template <typename T>
		T* GetContext() { return static_cast<T*>(Context); }

		virtual bool PrepareProcessing()
		{
			VtxIO->CreateInKeys();
			PCGExGraph::BuildEndpointsLookup(VtxIO, EndpointsLookup, ExpectedAdjacency);

			if (RequiresGraphBuilder())
			{
				GraphBuilder = new PCGExGraph::FGraphBuilder(VtxIO, &GraphBuilderSettings, 6, EdgeCollection);
			}

			return true;
		}

		virtual void Process(PCGExMT::FTaskManager* AsyncManager)
		{
			AsyncManagerPtr = AsyncManager;
		}

		virtual void ProcessClosedBatchRange(const int32 StartIndex, const int32 Iterations)
		{
		}

		void StartParallelLoopForRange(const int32 NumIterations, const int32 PerLoopIterations = -1)
		{
			int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetPointsBatchIteration(PerLoopIterations);
			int32 CurrentCount = 0;
			while (CurrentCount < NumIterations)
			{
				AsyncManagerPtr->Start<FAsyncProcessRange<FClusterProcessorBatchBase>>(
					CurrentCount, nullptr, this, FMath::Min(NumIterations - CurrentCount, PLI));
				CurrentCount += PLI;
			}
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

		virtual void Write()
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

		int32 GatherValidClusters()
		{
			ValidClusters.Empty();

			for (const T* P : Processors)
			{
				if (!P->Cluster) { continue; }
				ValidClusters.Add(P->Cluster);
			}
			return ValidClusters.Num();
		}

		virtual ~TBatch() override
		{
			PCGEX_DELETE_TARRAY(Processors)
			PCGEX_DELETE(GraphBuilder)

			ClosedBatchProcessors.Empty();
		}

		virtual bool PrepareProcessing() override
		{
			return FClusterProcessorBatchBase::PrepareProcessing();
		}

		virtual void Process(PCGExMT::FTaskManager* AsyncManager) override
		{
			FClusterProcessorBatchBase::Process(AsyncManager);

			if (VtxIO->GetNum() <= 1) { return; }

			CurrentState = PCGExMT::State_Processing;

			for (PCGExData::FPointIO* IO : Edges)
			{
				IO->CreateInKeys();

				T* NewProcessor = new T(VtxIO, IO);
				NewProcessor->Context = Context;
				NewProcessor->EndpointsLookup = &EndpointsLookup;
				NewProcessor->ExpectedAdjacency = &ExpectedAdjacency;
				NewProcessor->BatchIndex = Processors.Add(NewProcessor);
				NewProcessor->VtxDataCache = VtxDataCache;

				if (RequiresGraphBuilder()) { NewProcessor->GraphBuilder = GraphBuilder; }
				NewProcessor->SetRequiresHeuristics(RequiresHeuristics());

				if (!PrepareSingle(NewProcessor))
				{
					PCGEX_DELETE(NewProcessor)
					continue;
				}

				if (VtxFilterFactories) { NewProcessor->SetVtxFilterFactories(VtxFilterFactories); }

				if (IO->GetNum() < GetDefault<UPCGExGlobalSettings>()->SmallClusterSize)
				{
					NewProcessor->bIsSmallCluster = true;
					ClosedBatchProcessors.Add(NewProcessor);
				}

				if (bInlineProcessing) { NewProcessor->bIsProcessorValid = NewProcessor->Process(AsyncManagerPtr); }
				else if (!NewProcessor->IsTrivial()) { AsyncManager->Start<FAsyncProcessWithUpdate<T>>(IO->IOIndex, IO, NewProcessor); }
			}

			StartClosedBatchProcessing();
		}

		virtual bool PrepareSingle(T* ClusterProcessor) { return true; };

		virtual void CompleteWork() override
		{
			CurrentState = PCGExMT::State_Completing;

			if (bInlineCompletion)
			{
				for (T* Processor : Processors)
				{
					if (!Processor->bIsProcessorValid) { continue; }
					Processor->CompleteWork();
				}
			}
			else
			{
				for (T* Processor : Processors)
				{
					if (!Processor->bIsProcessorValid || Processor->IsTrivial()) { continue; }
					AsyncManagerPtr->Start<FAsyncCompleteWork<T>>(-1, nullptr, Processor);
				}

				StartClosedBatchProcessing();
			}
		}

		virtual void Write() override
		{
			CurrentState = PCGExMT::State_Writing;
			if (bInlineCompletion)
			{
				for (T* Processor : Processors)
				{
					if (!Processor->bIsProcessorValid) { continue; }
					Processor->Write();
				}
			}
			else
			{
				for (T* Processor : Processors)
				{
					if (!Processor->bIsProcessorValid || Processor->IsTrivial()) { continue; }
					PCGExMT::Write<T>(AsyncManagerPtr, Processor);
				}

				StartClosedBatchProcessing();
			}
		}

		virtual void ProcessClosedBatchRange(const int32 StartIndex, const int32 Iterations) override
		{
			if (CurrentState == PCGExMT::State_Processing)
			{
				for (int i = 0; i < Iterations; i++)
				{
					T* Processor = ClosedBatchProcessors[StartIndex + i];
					Processor->bIsProcessorValid = Processor->Process(AsyncManagerPtr);
				}
			}
			else if (CurrentState == PCGExMT::State_Completing)
			{
				for (int i = 0; i < Iterations; i++)
				{
					T* Processor = ClosedBatchProcessors[StartIndex + i];
					if (!Processor->bIsProcessorValid) { continue; }
					Processor->CompleteWork();
				}
			}
			else if (CurrentState == PCGExMT::State_Writing)
			{
				for (int i = 0; i < Iterations; i++)
				{
					T* Processor = ClosedBatchProcessors[StartIndex + i];
					if (!Processor->bIsProcessorValid) { continue; }
					Processor->Write();
				}
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
			this->bRequiresGraphBuilder = true;
		}
	};

	template <typename T>
	class TBatchWithHeuristics : public TBatch<T>
	{
	public:
		TBatchWithHeuristics(FPCGContext* InContext, PCGExData::FPointIO* InVtx, TArrayView<PCGExData::FPointIO*> InEdges):
			TBatch<T>(InContext, InVtx, InEdges)
		{
			this->SetRequiresHeuristics(true);
		}
	};

	template <typename T>
	class TBatchWithHeuristicsAndBuilder : public TBatch<T>
	{
	public:
		TBatchWithHeuristicsAndBuilder(FPCGContext* InContext, PCGExData::FPointIO* InVtx, TArrayView<PCGExData::FPointIO*> InEdges):
			TBatch<T>(InContext, InVtx, InEdges)
		{
			this->SetRequiresHeuristics(true);
			this->bRequiresGraphBuilder = true;
		}
	};

	static void ScheduleBatch(PCGExMT::FTaskManager* Manager, FClusterProcessorBatchBase* Batch)
	{
		Manager->Start<FStartClusterBatchProcessing<FClusterProcessorBatchBase>>(-1, nullptr, Batch);
	}

	static void CompleteBatch(PCGExMT::FTaskManager* Manager, FClusterProcessorBatchBase* Batch)
	{
		Manager->Start<FStartClusterBatchCompleteWork<FClusterProcessorBatchBase>>(-1, nullptr, Batch);
	}

	static void CompleteBatches(PCGExMT::FTaskManager* Manager, const TArrayView<FClusterProcessorBatchBase*> Batches)
	{
		for (FClusterProcessorBatchBase* Batch : Batches) { CompleteBatch(Manager, Batch); }
	}

	static void WriteBatch(PCGExMT::FTaskManager* Manager, FClusterProcessorBatchBase* Batch)
	{
		PCGEX_ASYNC_WRITE(Manager, Batch)
	}

	static void WriteBatches(PCGExMT::FTaskManager* Manager, const TArrayView<FClusterProcessorBatchBase*> Batches)
	{
		for (FClusterProcessorBatchBase* Batch : Batches) { WriteBatch(Manager, Batch); }
	}
}


#undef PCGEX_CLUSTER_MT_TASK
#undef PCGEX_CLUSTER_MT_TASK_RANGE
