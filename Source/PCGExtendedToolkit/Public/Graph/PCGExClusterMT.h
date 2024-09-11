// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExEdge.h"
#include "PCGExGraph.h"
#include "PCGExCluster.h"
#include "Data/PCGExClusterData.h"
#include "Data/PCGExData.h"
#include "Pathfinding/Heuristics/PCGExHeuristics.h"


namespace PCGExClusterMT
{
	PCGEX_ASYNC_STATE(MTState_ClusterProcessing)
	PCGEX_ASYNC_STATE(MTState_ClusterCompletingWork)
	PCGEX_ASYNC_STATE(MTState_ClusterWriting)

#pragma region Tasks

#define PCGEX_CLUSTER_MT_TASK(_NAME, _BODY)\
	template <typename T>\
	class /*PCGEXTENDEDTOOLKIT_API*/ _NAME final : public PCGExMT::FPCGExTask	{\
	public: _NAME(PCGExData::FPointIO* InPointIO, T* InTarget) : PCGExMT::FPCGExTask(InPointIO),Target(InTarget){} \
		T* Target = nullptr; virtual bool ExecuteTask() override{_BODY return true; }};

#define PCGEX_CLUSTER_MT_TASK_RANGE_INLINE(_NAME, _BODY)\
	template <typename T> \
	class /*PCGEXTENDEDTOOLKIT_API*/ _NAME final : public PCGExMT::FPCGExTask {\
	public: _NAME(PCGExData::FPointIO* InPointIO, T* InTarget, const uint64 InPerNumIterations, const uint64 InTotalIterations)\
	: PCGExMT::FPCGExTask(InPointIO), Target(InTarget), PerNumIterations(InPerNumIterations), TotalIterations(InTotalIterations){}\
		T* Target = nullptr; uint64 PerNumIterations = 0; uint64 TotalIterations = 0;\
		virtual bool ExecuteTask() override {\
		const uint64 RemainingIterations = TotalIterations - TaskIndex;\
		uint64 Iterations = FMath::Min(PerNumIterations, RemainingIterations); _BODY \
		int32 NextIndex = TaskIndex + Iterations; if (NextIndex >= TotalIterations) { return true; }\
		InternalStart<_NAME>(NextIndex, nullptr, Target, PerNumIterations, TotalIterations);\
		return true; } };

#define PCGEX_CLUSTER_MT_TASK_RANGE(_NAME, _BODY)\
	template <typename T>\
	class /*PCGEXTENDEDTOOLKIT_API*/ _NAME final : public PCGExMT::FPCGExTask	{\
	public: _NAME(PCGExData::FPointIO* InPointIO, T* InTarget, const uint64 InIterations) : PCGExMT::FPCGExTask(InPointIO),Target(InTarget), Iterations(InIterations){} \
		T* Target = nullptr; uint64 Iterations = 0; virtual bool ExecuteTask() override{_BODY return true; }};\
	PCGEX_CLUSTER_MT_TASK_RANGE_INLINE(_NAME##Inline, _BODY)

#define PCGEX_CLUSTER_MT_TASK_SCOPE(_NAME, _BODY)\
	template <typename T>\
	class /*PCGEXTENDEDTOOLKIT_API*/ _NAME final : public PCGExMT::FPCGExTask	{\
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

	PCGEX_CLUSTER_MT_TASK_SCOPE(FAsyncProcessScopeList, {for(const uint64 Scope: Scopes ){ Target->ProcessScope(Scope);}})

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

		bool bInlineProcessNodes = false;
		bool bInlineProcessEdges = false;
		bool bInlineProcessRange = false;


		int32 NumNodes = 0;
		int32 NumEdges = 0;

		virtual PCGExCluster::FCluster* HandleCachedCluster(const PCGExCluster::FCluster* InClusterRef)
		{
			return new PCGExCluster::FCluster(InClusterRef, VtxIO, EdgesIO, false, false, false);
		}

		void ForwardCluster(const bool bAsOwner)
		{
			if (UPCGExClusterEdgesData* EdgesData = Cast<UPCGExClusterEdgesData>(EdgesIO->GetOut()))
			{
				EdgesData->SetBoundCluster(Cluster, bAsOwner);
				bDeleteCluster = false;
			}
		}

	public:
		PCGExData::FFacade* VtxDataFacade = nullptr;
		PCGExData::FFacade* EdgeDataFacade = nullptr;

		bool bAllowFetchOnEdgesDataFacade = false;

		bool bIsProcessorValid = false;

		PCGExHeuristics::THeuristicsHandler* HeuristicsHandler = nullptr;

		bool bIsSmallCluster = false;
		bool bIsOneToOne = false;

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
			EdgeDataFacade = new PCGExData::FFacade(InEdges);
			EdgeDataFacade->bSupportsDynamic = bAllowFetchOnEdgesDataFacade;
		}

		virtual ~FClusterProcessor()
		{
			PCGEX_LOG_DTR(FClusterProcessor)

			PCGEX_DELETE(HeuristicsHandler);
			if (bDeleteCluster) { PCGEX_DELETE(Cluster); } // Safely delete the cluster

			VtxIO = nullptr;
			EdgesIO = nullptr;

			PCGEX_DELETE(EdgeDataFacade)
		}

		template <typename T>
		T* GetContext() { return static_cast<T*>(Context); }

		bool IsTrivial() const { return bIsSmallCluster; }

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
			}

			NumNodes = Cluster->Nodes->Num();
			NumEdges = Cluster->Edges->Num();

			if (bCacheVtxPointIndices) { VtxPointIndicesCache = Cluster->GetVtxPointIndicesPtr(); }

			if (bRequiresHeuristics)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FClusterProcessor::Heuristics);
				HeuristicsHandler = new PCGExHeuristics::THeuristicsHandler(Context, VtxDataFacade, EdgeDataFacade);
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
				for (int i = 0; i < NumNodes; ++i) { ProcessSingleNode(i, NodesRef[i]); }
				return;
			}

			const int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize(PerLoopIterations);
			if (bInlineProcessNodes)
			{
				AsyncManagerPtr->Start<FAsyncProcessNodeRangeInline<FClusterProcessor>>(
					0, nullptr, this, PLI, NumNodes);
				return;
			}

			PCGExMT::SubRanges(
				NumNodes, PLI,
				[&](const int32 Index, const int32 Count)
				{
					AsyncManagerPtr->Start<FAsyncProcessNodeRange<FClusterProcessor>>(
						Index, nullptr, this, Count);
				});
		}

		void StartParallelLoopForEdges(const int32 PerLoopIterations = -1)
		{
			if (IsTrivial())
			{
				for (PCGExGraph::FIndexedEdge& Edge : (*Cluster->Edges)) { ProcessSingleEdge(Edge); }
				return;
			}

			const int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize(PerLoopIterations);

			if (bInlineProcessEdges)
			{
				AsyncManagerPtr->Start<FAsyncProcessEdgeRangeInline<FClusterProcessor>>(
					0, nullptr, this, PLI, NumEdges);
				return;
			}

			PCGExMT::SubRanges(
				NumEdges, PLI,
				[&](const int32 Index, const int32 Count)
				{
					AsyncManagerPtr->Start<FAsyncProcessEdgeRange<FClusterProcessor>>(
						Index, nullptr, this, Count);
				});
		}

		void StartParallelLoopForRange(const int32 NumIterations, const int32 PerLoopIterations = -1)
		{
			if (IsTrivial())
			{
				for (int i = 0; i < NumIterations; ++i) { ProcessSingleRangeIteration(i); }
				return;
			}

			const int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize(PerLoopIterations);

			if (bInlineProcessRange)
			{
				AsyncManagerPtr->Start<FAsyncProcessRangeInline<FClusterProcessor>>(0, nullptr, this, PLI, NumIterations);
				return;
			}

			PCGExMT::SubRanges(
				NumIterations, PLI,
				[&](const int32 Index, const int32 Count)
				{
					AsyncManagerPtr->Start<FAsyncProcessRange<FClusterProcessor>>(Index, nullptr, this, Count);
				});
		}

		virtual void ProcessView(const int32 StartIndex, const TArrayView<PCGExCluster::FNode> NodeView)
		{
			for (int i = 0; i < NodeView.Num(); ++i)
			{
				const int32 Index = StartIndex + i;
				ProcessSingleNode(Index, *(Cluster->Nodes->GetData() + Index));
			}
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
			for (int i = 0; i < Iterations; ++i) { ProcessSingleRangeIteration(StartIndex + i); }
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

			int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize(-1);

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

		virtual void Output()
		{
		}
	};

	class FClusterProcessorBatchBase
	{
	protected:
		PCGExMT::FTaskManager* AsyncManagerPtr = nullptr;

		TMap<uint32, int32> EndpointsLookup;
		TArray<int32> ExpectedAdjacency;

		bool bRequiresHeuristics = false;
		bool bRequiresGraphBuilder = false;

	public:
		PCGExData::FFacade* VtxDataFacade = nullptr;
		bool bAllowFetchOnVtxDataFacade = false;

		bool bRequiresWriteStep = false;
		bool bWriteVtxDataFacade = false;

		mutable FRWLock BatchLock;

		FPCGContext* Context = nullptr;

		PCGExData::FPointIO* VtxIO = nullptr;
		TArray<PCGExData::FPointIO*> Edges;
		PCGExData::FPointIOCollection* EdgeCollection = nullptr;

		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
		FPCGExGraphBuilderDetails GraphBuilderDetails;

		TArray<PCGExCluster::FCluster*> ValidClusters;

		virtual int32 GetNumProcessors() const { return -1; }

		bool RequiresGraphBuilder() const { return bRequiresGraphBuilder; }
		bool RequiresHeuristics() const { return bRequiresHeuristics; }
		virtual void SetRequiresHeuristics(const bool bRequired = false) { bRequiresHeuristics = bRequired; }

		bool bInlineProcessing = false;
		bool bInlineCompletion = false;

		FClusterProcessorBatchBase(FPCGContext* InContext, PCGExData::FPointIO* InVtx, TArrayView<PCGExData::FPointIO*> InEdges):
			Context(InContext), VtxIO(InVtx)
		{
			Edges.Append(InEdges);
			VtxDataFacade = new PCGExData::FFacade(InVtx);
			VtxDataFacade->bSupportsDynamic = bAllowFetchOnVtxDataFacade;
		}

		virtual ~FClusterProcessorBatchBase()
		{
			Context = nullptr;
			VtxIO = nullptr;

			Edges.Empty();
			EndpointsLookup.Empty();
			ExpectedAdjacency.Empty();

			PCGEX_DELETE(GraphBuilder)
			PCGEX_DELETE(VtxDataFacade)
		}

		template <typename T>
		T* GetContext() { return static_cast<T*>(Context); }

		virtual bool PrepareProcessing()
		{
			VtxIO->CreateInKeys();
			PCGExGraph::BuildEndpointsLookup(VtxIO, EndpointsLookup, ExpectedAdjacency);

			if (RequiresGraphBuilder())
			{
				GraphBuilder = new PCGExGraph::FGraphBuilder(VtxIO, &GraphBuilderDetails, 6, EdgeCollection);
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
			PCGExMT::SubRanges(
				NumIterations, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize(PerLoopIterations),
				[&](const int32 Index, const int32 Count)
				{
					AsyncManagerPtr->Start<FAsyncProcessRange<FClusterProcessorBatchBase>>(Index, nullptr, this, Count);
				});
		}

		void ProcessRange(const int32 StartIndex, const int32 Iterations)
		{
			for (int i = 0; i < Iterations; ++i) { ProcessSingleRangeIteration(StartIndex + i); }
		}

		virtual void ProcessSingleRangeIteration(const int32 Iteration)
		{
		}

		virtual void CompleteWork()
		{
		}

		virtual void Write()
		{
			if (bWriteVtxDataFacade) { VtxDataFacade->Write(AsyncManagerPtr, true); }
		}

		virtual void Output()
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

		virtual int32 GetNumProcessors() const override { return Processors.Num(); }

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
				NewProcessor->VtxDataFacade = VtxDataFacade;

				if (RequiresGraphBuilder()) { NewProcessor->GraphBuilder = GraphBuilder; }
				NewProcessor->SetRequiresHeuristics(RequiresHeuristics());

				if (!PrepareSingle(NewProcessor))
				{
					PCGEX_DELETE(NewProcessor)
					continue;
				}

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

			FClusterProcessorBatchBase::Write();
		}

		virtual void ProcessClosedBatchRange(const int32 StartIndex, const int32 Iterations) override
		{
			if (CurrentState == PCGExMT::State_Processing)
			{
				for (int i = 0; i < Iterations; ++i)
				{
					T* Processor = ClosedBatchProcessors[StartIndex + i];
					Processor->bIsProcessorValid = Processor->Process(AsyncManagerPtr);
				}
			}
			else if (CurrentState == PCGExMT::State_Completing)
			{
				for (int i = 0; i < Iterations; ++i)
				{
					T* Processor = ClosedBatchProcessors[StartIndex + i];
					if (!Processor->bIsProcessorValid) { continue; }
					Processor->CompleteWork();
				}
			}
			else if (CurrentState == PCGExMT::State_Writing)
			{
				for (int i = 0; i < Iterations; ++i)
				{
					T* Processor = ClosedBatchProcessors[StartIndex + i];
					if (!Processor->bIsProcessorValid) { continue; }
					Processor->Write();
				}
			}
		}

		virtual void Output() override
		{
			for (T* Processor : Processors)
			{
				if (!Processor->bIsProcessorValid) { continue; }
				Processor->Output();
			}
		}

	protected:
		void StartClosedBatchProcessing()
		{
			const int32 NumTrivial = ClosedBatchProcessors.Num();
			if (NumTrivial > 0)
			{
				PCGExMT::SubRanges(
					NumTrivial,
					GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize(),
					[&](const int32 Index, const int32 Count)
					{
						AsyncManagerPtr->Start<FAsyncBatchProcessClosedRange<FClusterProcessorBatchBase>>(Index, nullptr, this, Count);
					});
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
#undef PCGEX_CLUSTER_MT_TASK_RANGE_INLINE
#undef PCGEX_CLUSTER_MT_TASK_RANGE
#undef PCGEX_CLUSTER_MT_TASK_SCOPE
