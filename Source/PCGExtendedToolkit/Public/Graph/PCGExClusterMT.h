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

	template <typename T>
	class FStartClusterBatchProcessing final : public PCGExMT::FPCGExTask
	{
	public:
		FStartClusterBatchProcessing(const TSharedPtr<PCGExData::FPointIO>& InPointIO, TSharedPtr<T> InTarget, bool bScoped) : FPCGExTask(InPointIO), Target(InTarget), bScopedIndexLookupBuild(bScoped)
		{
		}

		TSharedPtr<T> Target;
		bool bScopedIndexLookupBuild = false;

		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override
		{
			Target->PrepareProcessing(AsyncManager, bScopedIndexLookupBuild);
			return true;
		}
	};

#pragma endregion

	class FClusterProcessor : public TSharedFromThis<FClusterProcessor>
	{
		friend class FClusterProcessorBatchBase;

	protected:
		TSharedPtr<PCGExMT::FTaskManager> AsyncManager;
		FPCGExContext* ExecutionContext = nullptr;

		bool bBuildCluster = true;
		bool bRequiresHeuristics = false;
		bool bCacheVtxPointIndices = false;

		bool bInlineProcessNodes = false;
		bool bInlineProcessEdges = false;
		bool bInlineProcessRange = false;

		template <typename T>
		TSharedPtr<T> GetParentBatch() { return StaticCastSharedPtr<T>(ParentBatch); }

		int32 NumNodes = 0;
		int32 NumEdges = 0;

		virtual TSharedPtr<PCGExCluster::FCluster> HandleCachedCluster(const TSharedPtr<PCGExCluster::FCluster>& InClusterRef)
		{
			return MakeShared<PCGExCluster::FCluster>(InClusterRef.Get(), VtxIO, EdgesIO, false, false, false);
		}

		void ForwardCluster() const
		{
			if (UPCGExClusterEdgesData* EdgesData = Cast<UPCGExClusterEdgesData>(EdgesIO->GetOut()))
			{
				EdgesData->SetBoundCluster(Cluster);
			}
		}

	public:
		TSharedPtr<FClusterProcessorBatchBase> ParentBatch;

		TSharedPtr<PCGExData::FFacade> VtxDataFacade;
		TSharedPtr<PCGExData::FFacade> EdgeDataFacade;

		bool bAllowEdgesDataFacadeScopedGet = false;

		bool bIsProcessorValid = false;

		TSharedPtr<PCGExHeuristics::THeuristicsHandler> HeuristicsHandler;

		bool bIsTrivial = false;
		bool bIsOneToOne = false;

		const TArray<int32>* VtxPointIndicesCache = nullptr;

		TSharedPtr<PCGExData::FPointIO> VtxIO;
		TSharedPtr<PCGExData::FPointIO> EdgesIO;
		int32 BatchIndex = -1;

		TMap<uint32, int32>* EndpointsLookup = nullptr;
		TArray<int32>* ExpectedAdjacency = nullptr;

		TSharedPtr<PCGExCluster::FCluster> Cluster;

		TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder;

		FClusterProcessor(const TSharedPtr<PCGExData::FPointIO>& InVtx, const TSharedPtr<PCGExData::FPointIO>& InEdges):
			VtxIO(InVtx), EdgesIO(InEdges)
		{
			PCGEX_LOG_CTR(FClusterProcessor)
			EdgeDataFacade = MakeShared<PCGExData::FFacade>(InEdges);
			EdgeDataFacade->bSupportsScopedGet = bAllowEdgesDataFacadeScopedGet;
		}

		virtual void SetExecutionContext(FPCGExContext* InContext)
		{
			ExecutionContext = InContext;
		}

		virtual ~FClusterProcessor()
		{
			PCGEX_LOG_DTR(FClusterProcessor)
		}

		bool IsTrivial() const { return bIsTrivial; }

		void SetRequiresHeuristics(const bool bRequired = false) { bRequiresHeuristics = bRequired; }

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
		{
			AsyncManager = InAsyncManager;

			if (!bBuildCluster) { return true; }

			if (const TSharedPtr<PCGExCluster::FCluster> CachedCluster = PCGExClusterData::TryGetCachedCluster(VtxIO, EdgesIO))
			{
				Cluster = HandleCachedCluster(CachedCluster);
			}

			if (!Cluster)
			{
				Cluster = MakeShared<PCGExCluster::FCluster>();
				Cluster->bIsOneToOne = bIsOneToOne;
				Cluster->VtxIO = VtxIO;
				Cluster->EdgesIO = EdgesIO;

				if (!Cluster->BuildFrom(EdgesIO, VtxIO->GetIn()->GetPoints(), *EndpointsLookup, ExpectedAdjacency))
				{
					Cluster.Reset();
					return false;
				}
			}

			NumNodes = Cluster->Nodes->Num();
			NumEdges = Cluster->Edges->Num();

			if (bCacheVtxPointIndices) { VtxPointIndicesCache = Cluster->GetVtxPointIndicesPtr(); }

			if (bRequiresHeuristics)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FClusterProcessor::Heuristics);
				HeuristicsHandler = MakeShared<PCGExHeuristics::THeuristicsHandler>(ExecutionContext, VtxDataFacade, EdgeDataFacade);
				HeuristicsHandler->PrepareForCluster(Cluster.Get());
				HeuristicsHandler->CompleteClusterPreparation();
			}

			return true;
		}

#pragma region Parallel loops

		void StartParallelLoopForNodes(const int32 PerLoopIterations = -1)
		{
			if (IsTrivial())
			{
				PrepareLoopScopesForNodes({PCGEx::H64(0, NumNodes)});
				ProcessNodes(0, NumNodes, 0);
				OnNodesProcessingComplete();
				return;
			}

			const int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize(PerLoopIterations);

			PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ParallelLoopForNodes)
			ParallelLoopForNodes->SetOnCompleteCallback([&]() { OnNodesProcessingComplete(); });
			ParallelLoopForNodes->SetOnIterationRangePrepareCallback([&](const TArray<uint64>& Loops) { PrepareLoopScopesForNodes(Loops); });
			ParallelLoopForNodes->SetOnIterationRangeStartCallback(
				[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx) { ProcessNodes(StartIndex, Count, LoopIdx); });
			ParallelLoopForNodes->PrepareRangesOnly(NumNodes, PLI, bInlineProcessNodes);
		}

		virtual void PrepareLoopScopesForNodes(const TArray<uint64>& Loops)
		{
		}

		virtual void PrepareSingleLoopScopeForNodes(const uint32 StartIndex, const int32 Count)
		{
		}

		virtual void ProcessNodes(const int32 StartIndex, const int32 Count, const int32 LoopIdx)
		{
			PrepareSingleLoopScopeForNodes(StartIndex, Count);
			TArray<PCGExCluster::FNode>& Nodes = *Cluster->Nodes;
			for (int i = 0; i < Count; ++i)
			{
				const int32 PtIndex = StartIndex + i;
				ProcessSingleNode(PtIndex, Nodes[PtIndex], LoopIdx, Count);
			}
		}

		virtual void ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count)
		{
		}

		virtual void OnNodesProcessingComplete()
		{
		}


		void StartParallelLoopForEdges(const int32 PerLoopIterations = -1)
		{
			if (IsTrivial())
			{
				PrepareLoopScopesForEdges({PCGEx::H64(0, NumEdges)});
				ProcessEdges(0, NumEdges, 0);
				OnEdgesProcessingComplete();
				return;
			}

			const int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize(PerLoopIterations);

			PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ParallelLoopForEdges)
			ParallelLoopForEdges->SetOnCompleteCallback([&]() { OnEdgesProcessingComplete(); });
			ParallelLoopForEdges->SetOnIterationRangePrepareCallback([&](const TArray<uint64>& Loops) { PrepareLoopScopesForEdges(Loops); });
			ParallelLoopForEdges->SetOnIterationRangeStartCallback(
				[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx) { ProcessEdges(StartIndex, Count, LoopIdx); });
			ParallelLoopForEdges->PrepareRangesOnly(NumEdges, PLI, bInlineProcessEdges);
		}

		virtual void PrepareLoopScopesForEdges(const TArray<uint64>& Loops)
		{
		}

		virtual void PrepareSingleLoopScopeForEdges(const uint32 StartIndex, const int32 Count)
		{
		}

		virtual void ProcessEdges(const int32 StartIndex, const int32 Count, const int32 LoopIdx)
		{
			PrepareSingleLoopScopeForEdges(StartIndex, Count);
			TArray<PCGExGraph::FIndexedEdge>& ClusterEdges = *Cluster->Edges;
			for (int i = 0; i < Count; ++i)
			{
				const int32 PtIndex = StartIndex + i;
				ProcessSingleEdge(PtIndex, ClusterEdges[PtIndex], LoopIdx, Count);
			}
		}

		virtual void ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count)
		{
		}

		virtual void OnEdgesProcessingComplete()
		{
		}

		void StartParallelLoopForRange(const int32 NumIterations, const int32 PerLoopIterations = -1)
		{
			if (IsTrivial())
			{
				PrepareLoopScopesForRanges({PCGEx::H64(0, NumIterations)});
				ProcessRange(0, NumIterations, 0);
				OnRangeProcessingComplete();
				return;
			}

			const int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize(PerLoopIterations);

			PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ParallelLoopForRanges)
			ParallelLoopForRanges->SetOnCompleteCallback([&]() { OnRangeProcessingComplete(); });
			ParallelLoopForRanges->SetOnIterationRangePrepareCallback([&](const TArray<uint64>& Loops) { PrepareLoopScopesForRanges(Loops); });
			ParallelLoopForRanges->SetOnIterationRangeStartCallback(
				[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx) { ProcessRange(StartIndex, Count, LoopIdx); });
			ParallelLoopForRanges->PrepareRangesOnly(NumIterations, PLI, bInlineProcessRange);
		}

		virtual void PrepareLoopScopesForRanges(const TArray<uint64>& Loops)
		{
		}

		virtual void PrepareSingleLoopScopeForRange(const uint32 StartIndex, const int32 Count)
		{
		}

		virtual void ProcessRange(const int32 StartIndex, const int32 Count, const int32 LoopIdx)
		{
			PrepareSingleLoopScopeForRange(StartIndex, Count);
			for (int i = 0; i < Count; ++i) { ProcessSingleRangeIteration(StartIndex + i, LoopIdx, Count); }
		}

		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 Count)
		{
		}

		virtual void OnRangeProcessingComplete()
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

	template <typename TContext, typename TSettings>
	class TClusterProcessor : public FClusterProcessor
	{
	protected:
		TContext* Context = nullptr;
		const TSettings* Settings = nullptr;

	public:
		TClusterProcessor(const TSharedPtr<PCGExData::FPointIO>& InVtx, const TSharedPtr<PCGExData::FPointIO>& InEdges):
			FClusterProcessor(InVtx, InEdges)
		{
		}

		virtual void SetExecutionContext(FPCGExContext* InContext) override
		{
			FClusterProcessor::SetExecutionContext(InContext);
			Context = static_cast<TContext*>(ExecutionContext);
			Settings = InContext->GetInputSettings<TSettings>();
		}

		FORCEINLINE TContext* GetContext() { return Context; }
		FORCEINLINE const TSettings* GetSettings() { return Settings; }
	};

	class FClusterProcessorBatchBase : public TSharedFromThis<FClusterProcessorBatchBase>
	{
	protected:
		TSharedPtr<PCGExMT::FTaskManager> AsyncManager;

		const FPCGMetadataAttribute<int64>* RawLookupAttribute = nullptr;
		TArray<uint32> ReverseLookup;

		TMap<uint32, int32> EndpointsLookup;
		TArray<int32> ExpectedAdjacency;

		bool bPreparationSuccessful = false;
		bool bRequiresHeuristics = false;
		bool bRequiresGraphBuilder = false;

	public:
		TSharedPtr<PCGExData::FFacade> VtxDataFacade;
		bool bAllowVtxDataFacadeScopedGet = false;

		bool bRequiresWriteStep = false;
		bool bWriteVtxDataFacade = false;

		mutable FRWLock BatchLock;

		FPCGExContext* ExecutionContext = nullptr;

		TSharedPtr<PCGExData::FPointIO> VtxIO;
		TArray<TSharedPtr<PCGExData::FPointIO>> Edges;
		TSharedPtr<PCGExData::FPointIOCollection> EdgeCollection;

		TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder;
		FPCGExGraphBuilderDetails GraphBuilderDetails;

		TArray<PCGExCluster::FCluster*> ValidClusters;

		virtual int32 GetNumProcessors() const { return -1; }

		bool PreparationSuccessful() const { return bPreparationSuccessful; }
		bool RequiresGraphBuilder() const { return bRequiresGraphBuilder; }
		bool RequiresHeuristics() const { return bRequiresHeuristics; }
		virtual void SetRequiresHeuristics(const bool bRequired = false) { bRequiresHeuristics = bRequired; }

		bool bInlineProcessing = false;
		bool bInlineCompletion = false;
		bool bInlineWrite = false;

		FClusterProcessorBatchBase(FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIO>& InVtx, TArrayView<TSharedPtr<PCGExData::FPointIO>> InEdges):
			ExecutionContext(InContext), VtxIO(InVtx)
		{
			Edges.Append(InEdges);
			VtxDataFacade = MakeShared<PCGExData::FFacade>(InVtx);
			VtxDataFacade->bSupportsScopedGet = bAllowVtxDataFacadeScopedGet;
		}

		virtual ~FClusterProcessorBatchBase()
		{
			Edges.Empty();
			EndpointsLookup.Empty();
			ExpectedAdjacency.Empty();
		}

		template <typename T>
		T* GetContext() { return static_cast<T*>(ExecutionContext); }

		virtual void PrepareProcessing(TSharedPtr<PCGExMT::FTaskManager> AsyncManagerPtr, const bool bScopedIndexLookupBuild)
		{
			AsyncManager = AsyncManagerPtr;

			VtxIO->CreateInKeys();
			const int32 NumVtx = VtxIO->GetNum();

			if (!bScopedIndexLookupBuild || NumVtx < GetDefault<UPCGExGlobalSettings>()->SmallClusterSize)
			{
				// Trivial
				PCGExGraph::BuildEndpointsLookup(VtxIO, EndpointsLookup, ExpectedAdjacency);
				if (RequiresGraphBuilder())
				{
					GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(VtxDataFacade, &GraphBuilderDetails, 6, EdgeCollection);
				}

				OnProcessingPreparationComplete();
			}
			else
			{
				// Spread
				PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManagerPtr, BuildEndpointLookupTask)

				PCGEX_SET_NUM_UNINITIALIZED(ReverseLookup, NumVtx)
				PCGEX_SET_NUM_UNINITIALIZED(ExpectedAdjacency, NumVtx)

				RawLookupAttribute = VtxIO->GetIn()->Metadata->GetConstTypedAttribute<int64>(PCGExGraph::Tag_VtxEndpoint);
				if (!RawLookupAttribute) { return; } // FAIL

				BuildEndpointLookupTask->SetOnCompleteCallback(
					[&]()
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExGraph::BuildLookupTable::Complete);

						const int32 Num = VtxIO->GetNum();
						EndpointsLookup.Reserve(Num);
						for (int i = 0; i < Num; i++) { EndpointsLookup.Add(ReverseLookup[i], i); }
						ReverseLookup.Empty();

						if (RequiresGraphBuilder())
						{
							GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(VtxDataFacade, &GraphBuilderDetails, 6, EdgeCollection);
						}

						OnProcessingPreparationComplete();
					});

				BuildEndpointLookupTask->SetOnIterationRangeStartCallback(
					[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExGraph::BuildLookupTable::Range);

						const TArray<FPCGPoint>& InKeys = VtxIO->GetIn()->GetPoints();

						const int32 MaxIndex = StartIndex + Count;
						for (int i = StartIndex; i < MaxIndex; i++)
						{
							uint32 A;
							uint32 B;
							PCGEx::H64(RawLookupAttribute->GetValueFromItemKey(InKeys[i].MetadataEntry), A, B);

							ReverseLookup[i] = A;
							ExpectedAdjacency[i] = B;
						}
					});
				BuildEndpointLookupTask->PrepareRangesOnly(VtxIO->GetNum(), 4096);
			}
		}

		virtual void OnProcessingPreparationComplete()
		{
			Process();
		}

		virtual void Process()
		{
		}

		virtual void CompleteWork()
		{
		}

		virtual void Write()
		{
			if (bWriteVtxDataFacade) { VtxDataFacade->Write(AsyncManager); }
		}

		virtual void Output()
		{
		}
	};

	template <typename T>
	class TBatch : public FClusterProcessorBatchBase
	{
	public:
		TArray<TSharedPtr<T>> Processors;
		TArray<TSharedPtr<T>> TrivialProcessors;

		PCGExMT::AsyncState CurrentState = PCGExMT::State_Setup;

		virtual int32 GetNumProcessors() const override { return Processors.Num(); }

		TBatch(FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedPtr<PCGExData::FPointIO>> InEdges):
			FClusterProcessorBatchBase(InContext, InVtx, InEdges)
		{
		}

		int32 GatherValidClusters()
		{
			ValidClusters.Empty();

			for (const TSharedPtr<T> P : Processors)
			{
				if (!P->Cluster) { continue; }
				ValidClusters.Add(P->Cluster.Get());
			}
			return ValidClusters.Num();
		}

		virtual ~TBatch() override
		{
		}

		virtual void Process() override
		{
			FClusterProcessorBatchBase::Process();

			if (VtxIO->GetNum() <= 1) { return; }

			CurrentState = PCGExMT::State_Processing;

			TSharedPtr<FClusterProcessorBatchBase> SelfShared = SharedThis(this);

			for (const TSharedPtr<PCGExData::FPointIO>& IO : Edges)
			{
				IO->CreateInKeys();

				TSharedPtr<T> NewProcessor = Processors.Add_GetRef(MakeShared<T>(VtxIO, IO));
				NewProcessor->SetExecutionContext(ExecutionContext);
				NewProcessor->ParentBatch = SelfShared;
				NewProcessor->EndpointsLookup = &EndpointsLookup;
				NewProcessor->ExpectedAdjacency = &ExpectedAdjacency;
				NewProcessor->BatchIndex = Processors.Num() - 1;
				NewProcessor->VtxDataFacade = VtxDataFacade;

				if (RequiresGraphBuilder()) { NewProcessor->GraphBuilder = GraphBuilder; }
				NewProcessor->SetRequiresHeuristics(RequiresHeuristics());

				if (!PrepareSingle(NewProcessor))
				{
					Processors.Pop();
					continue;
				}

				NewProcessor->bIsTrivial = IO->GetNum() < GetDefault<UPCGExGlobalSettings>()->SmallClusterSize;
				if (NewProcessor->IsTrivial()) { TrivialProcessors.Add(NewProcessor); }
			}

			StartProcessing();
		}

		virtual void StartProcessing()
		{
			PCGEX_ASYNC_MT_LOOP_TPL(Process, bInlineProcessing, { Processor->bIsProcessorValid = Processor->Process(AsyncManager); })
		}

		virtual bool PrepareSingle(const TSharedPtr<T>& ClusterProcessor) { return true; }

		virtual void CompleteWork() override
		{
			CurrentState = PCGExMT::State_Completing;
			PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(CompleteWork, bInlineCompletion, { Processor->CompleteWork(); })
			FClusterProcessorBatchBase::CompleteWork();
		}

		virtual void Write() override
		{
			CurrentState = PCGExMT::State_Writing;
			PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(Write, bInlineWrite, { Processor->Write(); })
			FClusterProcessorBatchBase::Write();
		}

		virtual void Output() override
		{
			for (const TSharedPtr<T>& P : Processors)
			{
				if (!P->bIsProcessorValid) { continue; }
				P->Output();
			}
		}
	};

	template <typename T>
	class TBatchWithGraphBuilder : public TBatch<T>
	{
	public:
		TBatchWithGraphBuilder(FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIO>& InVtx, TArrayView<TSharedPtr<PCGExData::FPointIO>> InEdges):
			TBatch<T>(InContext, InVtx, InEdges)
		{
			this->bRequiresGraphBuilder = true;
		}
	};

	template <typename T>
	class TBatchWithHeuristics : public TBatch<T>
	{
	public:
		TBatchWithHeuristics(FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIO>& InVtx, TArrayView<TSharedPtr<PCGExData::FPointIO>> InEdges):
			TBatch<T>(InContext, InVtx, InEdges)
		{
			this->SetRequiresHeuristics(true);
		}
	};

	template <typename T>
	class TBatchWithHeuristicsAndBuilder : public TBatch<T>
	{
	public:
		TBatchWithHeuristicsAndBuilder(FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIO>& InVtx, TArrayView<TSharedPtr<PCGExData::FPointIO>> InEdges):
			TBatch<T>(InContext, InVtx, InEdges)
		{
			this->SetRequiresHeuristics(true);
			this->bRequiresGraphBuilder = true;
		}
	};

	static void ScheduleBatch(const TSharedPtr<PCGExMT::FTaskManager>& Manager, const TSharedPtr<FClusterProcessorBatchBase>& Batch, const bool bScopedIndexLookupBuild)
	{
		Manager->Start<FStartClusterBatchProcessing<FClusterProcessorBatchBase>>(-1, nullptr, Batch, bScopedIndexLookupBuild);
	}

	static void CompleteBatches(const TArrayView<TSharedPtr<FClusterProcessorBatchBase>> Batches)
	{
		for (const TSharedPtr<FClusterProcessorBatchBase>& Batch : Batches) { Batch->CompleteWork(); }
	}

	static void WriteBatches(const TArrayView<TSharedPtr<FClusterProcessorBatchBase>> Batches)
	{
		for (const TSharedPtr<FClusterProcessorBatchBase>& Batch : Batches) { Batch->Write(); }
	}
}
