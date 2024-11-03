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

#define PCGEX_ASYNC_CLUSTER_PROCESSOR_LOOP(_TYPE, _NAME, _NUM, _PREPARE, _PROCESS, _COMPLETE, _INLINE) PCGEX_ASYNC_PROCESSOR_LOOP(_TYPE, _NAME, _NUM, _PREPARE, _PROCESS, _COMPLETE, _INLINE, GetClusterBatchChunkSize)

	template <typename T>
	class FStartClusterBatchProcessing final : public PCGExMT::FPCGExTask
	{
	public:
		FStartClusterBatchProcessing(const TSharedPtr<PCGExData::FPointIO>& InPointIO, TSharedPtr<T> InTarget, const bool bScoped) : FPCGExTask(InPointIO), Target(InTarget), bScopedIndexLookupBuild(bScoped)
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

	class FClusterProcessorBatchBase;

	class FClusterProcessor : public TSharedFromThis<FClusterProcessor>
	{
		friend class FClusterProcessorBatchBase;

	protected:
		TSharedPtr<PCGExMT::FTaskManager> AsyncManager;
		FPCGExContext* ExecutionContext = nullptr;


		const TArray<TObjectPtr<const UPCGExHeuristicsFactoryBase>>* HeuristicsFactories = nullptr;
		FPCGExEdgeDirectionSettings DirectionSettings;

		bool bBuildCluster = true;
		bool bRequiresHeuristics = false;

		bool bInlineProcessNodes = false;
		bool bInlineProcessEdges = false;
		bool bInlineProcessRange = false;

		int32 NumNodes = 0;
		int32 NumEdges = 0;

		virtual TSharedPtr<PCGExCluster::FCluster> HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef)
		{
			return MakeShared<PCGExCluster::FCluster>(
				InClusterRef, VtxDataFacade->Source, EdgeDataFacade->Source, NodeIndexLookup,
				false, false, false);
		}

		void ForwardCluster() const
		{
			if (UPCGExClusterEdgesData* EdgesData = Cast<UPCGExClusterEdgesData>(EdgeDataFacade->GetOut()))
			{
				EdgesData->SetBoundCluster(Cluster);
			}
		}

	public:
		TSharedRef<PCGExData::FFacade> VtxDataFacade;
		TSharedRef<PCGExData::FFacade> EdgeDataFacade;

		TSharedPtr<PCGEx::FIndexLookup> NodeIndexLookup;

		TWeakPtr<FClusterProcessorBatchBase> ParentBatch;
		TSharedPtr<PCGExMT::FTaskManager> GetAsyncManager() { return AsyncManager; }

		bool bAllowEdgesDataFacadeScopedGet = false;

		bool bIsProcessorValid = false;

		TSharedPtr<PCGExHeuristics::FHeuristicsHandler> HeuristicsHandler;

		bool bIsTrivial = false;
		bool bIsOneToOne = false;

		int32 BatchIndex = -1;

		TMap<uint32, int32>* EndpointsLookup = nullptr;
		TArray<int32>* ExpectedAdjacency = nullptr;

		TSharedPtr<PCGExCluster::FCluster> Cluster;

		TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder;

		FClusterProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			VtxDataFacade(InVtxDataFacade), EdgeDataFacade(InEdgeDataFacade)
		{
			PCGEX_LOG_CTR(FClusterProcessor)
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

		void SetRequiresHeuristics(const bool bRequired, const TArray<TObjectPtr<const UPCGExHeuristicsFactoryBase>>* InHeuristicsFactories)
		{
			HeuristicsFactories = InHeuristicsFactories;
			bRequiresHeuristics = bRequired;
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
		{
			AsyncManager = InAsyncManager;

			if (!bBuildCluster) { return true; }

			if (const TSharedPtr<PCGExCluster::FCluster> CachedCluster = PCGExClusterData::TryGetCachedCluster(VtxDataFacade->Source, EdgeDataFacade->Source))
			{
				Cluster = HandleCachedCluster(CachedCluster.ToSharedRef());
			}

			if (!Cluster)
			{
				Cluster = MakeShared<PCGExCluster::FCluster>(VtxDataFacade->Source, EdgeDataFacade->Source, NodeIndexLookup);
				Cluster->bIsOneToOne = bIsOneToOne;

				if (!Cluster->BuildFrom(*EndpointsLookup, ExpectedAdjacency))
				{
					Cluster.Reset();
					return false;
				}
			}

			NumNodes = Cluster->Nodes->Num();
			NumEdges = Cluster->Edges->Num();

			if (bRequiresHeuristics)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FClusterProcessor::Heuristics);
				HeuristicsHandler = MakeShared<PCGExHeuristics::FHeuristicsHandler>(ExecutionContext, VtxDataFacade, EdgeDataFacade, *HeuristicsFactories);

				if (!HeuristicsHandler->IsValidHandler()) { return false; }

				HeuristicsHandler->PrepareForCluster(Cluster);
				HeuristicsHandler->CompleteClusterPreparation();
			}

			return true;
		}

#pragma region Parallel loops

		void StartParallelLoopForNodes(const int32 PerLoopIterations = -1)
		{
			PCGEX_ASYNC_CLUSTER_PROCESSOR_LOOP(
				FClusterProcessor, Nodes, NumNodes,
				PrepareLoopScopesForNodes, ProcessNodes,
				OnNodesProcessingComplete,
				bInlineProcessNodes)
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
			for (int i = 0; i < Count; i++)
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
			PCGEX_ASYNC_CLUSTER_PROCESSOR_LOOP(
				FClusterProcessor, Edges, NumEdges,
				PrepareLoopScopesForEdges, ProcessEdges,
				OnEdgesProcessingComplete,
				bInlineProcessEdges)
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
			for (int i = 0; i < Count; i++)
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
			PCGEX_ASYNC_CLUSTER_PROCESSOR_LOOP(
				FClusterProcessor, Ranges, NumIterations,
				PrepareLoopScopesForRanges, ProcessRange,
				OnRangeProcessingComplete,
				bInlineProcessRange)
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
			for (int i = 0; i < Count; i++) { ProcessSingleRangeIteration(StartIndex + i, LoopIdx, Count); }
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

		virtual void Cleanup()
		{
			bIsProcessorValid = false;
		}
	};

	template <typename TContext, typename TSettings>
	class TProcessor : public FClusterProcessor
	{
	protected:
		TContext* Context = nullptr;
		const TSettings* Settings = nullptr;

	public:
		TProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			FClusterProcessor(InVtxDataFacade, InEdgeDataFacade)
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
		mutable FRWLock BatchLock;
		TSharedPtr<PCGEx::FIndexLookup> NodeIndexLookup;

		TSharedPtr<PCGExMT::FTaskManager> AsyncManager;
		TSharedPtr<PCGExData::FFacadePreloader> VtxFacadePreloader;

		const FPCGMetadataAttribute<int64>* RawLookupAttribute = nullptr;
		TArray<uint32> ReverseLookup;

		TMap<uint32, int32> EndpointsLookup;
		TArray<int32> ExpectedAdjacency;

		bool bPreparationSuccessful = false;
		bool bRequiresHeuristics = false;
		bool bRequiresGraphBuilder = false;

	public:
		bool bIsBatchValid = true;
		FPCGExContext* ExecutionContext = nullptr;
		const TArray<TObjectPtr<const UPCGExHeuristicsFactoryBase>>* HeuristicsFactories = nullptr;

		const TSharedRef<PCGExData::FFacade> VtxDataFacade;
		bool bAllowVtxDataFacadeScopedGet = false;

		bool bRequiresWriteStep = false;
		bool bWriteVtxDataFacade = false;

		TArray<TSharedPtr<PCGExData::FPointIO>> Edges;
		TSharedPtr<PCGExData::FPointIOCollection> EdgeCollection;

		TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder;
		FPCGExGraphBuilderDetails GraphBuilderDetails;

		TArray<TSharedPtr<PCGExCluster::FCluster>> ValidClusters;

		virtual int32 GetNumProcessors() const { return -1; }

		bool PreparationSuccessful() const { return bPreparationSuccessful; }
		bool RequiresGraphBuilder() const { return bRequiresGraphBuilder; }
		bool RequiresHeuristics() const { return bRequiresHeuristics; }
		virtual void SetRequiresHeuristics(const bool bRequired) { bRequiresHeuristics = bRequired; }

		bool bInlineProcessing = false;
		bool bInlineCompletion = false;
		bool bInlineWrite = false;

		FClusterProcessorBatchBase(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges):
			ExecutionContext(InContext), VtxDataFacade(MakeShared<PCGExData::FFacade>(InVtx))
		{
			PCGEX_LOG_CTR(FClusterProcessorBatchBase)
			Edges.Append(InEdges);
		}

		virtual ~FClusterProcessorBatchBase()
		{
			PCGEX_LOG_DTR(FClusterProcessorBatchBase)
		}

		template <typename T>
		T* GetContext() { return static_cast<T*>(ExecutionContext); }

		virtual void PrepareProcessing(const TSharedPtr<PCGExMT::FTaskManager> AsyncManagerPtr, const bool bScopedIndexLookupBuild)
		{
			AsyncManager = AsyncManagerPtr;
			VtxDataFacade->bSupportsScopedGet = bAllowVtxDataFacadeScopedGet && ExecutionContext->bScopedAttributeGet;

			const int32 NumVtx = VtxDataFacade->GetNum();
			NodeIndexLookup = MakeShared<PCGEx::FIndexLookup>(NumVtx);

			if (!bScopedIndexLookupBuild || NumVtx < GetDefault<UPCGExGlobalSettings>()->SmallClusterSize)
			{
				// Trivial
				PCGExGraph::BuildEndpointsLookup(VtxDataFacade->Source, EndpointsLookup, ExpectedAdjacency);
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

				PCGEx::InitArray(ReverseLookup, NumVtx);
				PCGEx::InitArray(ExpectedAdjacency, NumVtx);

				RawLookupAttribute = VtxDataFacade->GetIn()->Metadata->GetConstTypedAttribute<int64>(PCGExGraph::Tag_VtxEndpoint);
				if (!RawLookupAttribute) { return; } // FAIL

				BuildEndpointLookupTask->OnCompleteCallback =
					[WeakThis = TWeakPtr<FClusterProcessorBatchBase>(SharedThis(this))]()
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExGraph::BuildLookupTable::Complete);

						const TSharedPtr<FClusterProcessorBatchBase> This = WeakThis.Pin();
						if (!This) { return; }

						const int32 Num = This->VtxDataFacade->GetNum();
						This->EndpointsLookup.Reserve(Num);
						for (int i = 0; i < Num; i++) { This->EndpointsLookup.Add(This->ReverseLookup[i], i); }
						This->ReverseLookup.Empty();

						if (This->RequiresGraphBuilder())
						{
							This->GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(This->VtxDataFacade, &This->GraphBuilderDetails, 6, This->EdgeCollection);
						}

						This->OnProcessingPreparationComplete();
					};

				BuildEndpointLookupTask->OnSubLoopStartCallback =
					[WeakThis = TWeakPtr<FClusterProcessorBatchBase>(SharedThis(this))]
					(const int32 StartIndex, const int32 Count, const int32 LoopIdx)
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExGraph::BuildLookupTable::Range);

						const TSharedPtr<FClusterProcessorBatchBase> This = WeakThis.Pin();
						if (!This) { return; }

						const TArray<FPCGPoint>& InKeys = This->VtxDataFacade->GetIn()->GetPoints();

						const int32 MaxIndex = StartIndex + Count;
						for (int i = StartIndex; i < MaxIndex; i++)
						{
							uint32 A;
							uint32 B;
							PCGEx::H64(This->RawLookupAttribute->GetValueFromItemKey(InKeys[i].MetadataEntry), A, B);

							This->ReverseLookup[i] = A;
							This->ExpectedAdjacency[i] = B;
						}
					};
				BuildEndpointLookupTask->StartSubLoops(VtxDataFacade->GetNum(), 4096);
			}
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
		{
		}

		virtual void OnProcessingPreparationComplete()
		{
			if (!bIsBatchValid) { return; }

			VtxFacadePreloader = MakeShared<PCGExData::FFacadePreloader>();
			RegisterBuffersDependencies(*VtxFacadePreloader);

			TWeakPtr<FClusterProcessorBatchBase> WeakPtr = SharedThis(this);
			VtxFacadePreloader->OnCompleteCallback = [WeakPtr]
			{
				const TSharedPtr<FClusterProcessorBatchBase> This = WeakPtr.Pin();
				if (!This) { return; }
				This->Process();
			};

			VtxFacadePreloader->StartLoading(AsyncManager, VtxDataFacade);
		}

		virtual void Process()
		{
		}

		virtual void CompleteWork()
		{
		}

		virtual void Write()
		{
			if (bWriteVtxDataFacade && bIsBatchValid) { VtxDataFacade->Write(AsyncManager); }
		}

		virtual const PCGExGraph::FGraphMetadataDetails* GetGraphMetadataDetails() { return nullptr; }

		virtual void CompileGraphBuilder(const bool bOutputToContext)
		{
			if (!GraphBuilder || !bIsBatchValid) { return; }

			if (bOutputToContext)
			{
				GraphBuilder->OnCompilationEndCallback = [&](const TSharedRef<PCGExGraph::FGraphBuilder>& InBuilder, const bool bSuccess)
				{
					if (!bSuccess)
					{
						// TODO : Log error
						return;
					}

					InBuilder->StageEdgesOutputs();
				};
			}

			GraphBuilder->CompileAsync(AsyncManager, true, GetGraphMetadataDetails());
		}

		virtual void Output()
		{
		}

		virtual void Cleanup()
		{
		}
	};

	template <typename T>
	class TBatch : public FClusterProcessorBatchBase
	{
	public:
		TArray<TSharedRef<T>> Processors;
		TArray<TSharedRef<T>> TrivialProcessors;

		PCGEx::AsyncState CurrentState = PCGEx::State_InitialExecution;

		virtual int32 GetNumProcessors() const override { return Processors.Num(); }

		TBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges):
			FClusterProcessorBatchBase(InContext, InVtx, InEdges)
		{
		}

		int32 GatherValidClusters()
		{
			ValidClusters.Empty();

			for (const TSharedPtr<T> P : Processors)
			{
				if (!P->Cluster) { continue; }
				ValidClusters.Add(P->Cluster);
			}
			return ValidClusters.Num();
		}

		virtual void Process() override
		{
			if (!bIsBatchValid) { return; }

			FClusterProcessorBatchBase::Process();

			if (VtxDataFacade->GetNum() <= 1) { return; }

			CurrentState = PCGEx::State_Processing;
			TSharedPtr<FClusterProcessorBatchBase> SelfPtr = SharedThis(this);

			for (const TSharedPtr<PCGExData::FPointIO>& IO : Edges)
			{
				const TSharedPtr<PCGExData::FFacade> EdgeDataFacade = MakeShared<PCGExData::FFacade>(IO.ToSharedRef());
				const TSharedPtr<T> NewProcessor = MakeShared<T>(VtxDataFacade, EdgeDataFacade.ToSharedRef());

				NewProcessor->SetExecutionContext(ExecutionContext);
				NewProcessor->ParentBatch = SelfPtr;
				NewProcessor->NodeIndexLookup = NodeIndexLookup;
				NewProcessor->EndpointsLookup = &EndpointsLookup;
				NewProcessor->ExpectedAdjacency = &ExpectedAdjacency;
				NewProcessor->BatchIndex = Processors.Num();

				if (RequiresGraphBuilder()) { NewProcessor->GraphBuilder = GraphBuilder; }
				NewProcessor->SetRequiresHeuristics(RequiresHeuristics(), HeuristicsFactories);

				if (!PrepareSingle(NewProcessor)) { continue; }
				Processors.Add(NewProcessor.ToSharedRef());

				NewProcessor->bIsTrivial = IO->GetNum() < GetDefault<UPCGExGlobalSettings>()->SmallClusterSize;
				if (NewProcessor->IsTrivial()) { TrivialProcessors.Add(NewProcessor.ToSharedRef()); }
			}

			StartProcessing();
		}

		virtual void StartProcessing()
		{
			if (!bIsBatchValid) { return; }

			PCGEX_ASYNC_MT_LOOP_TPL(Process, bInlineProcessing, { Processor->bIsProcessorValid = Processor->Process(Batch->AsyncManager); })
		}

		virtual bool PrepareSingle(const TSharedPtr<T>& ClusterProcessor) { return true; }

		virtual void CompleteWork() override
		{
			if (!bIsBatchValid) { return; }

			CurrentState = PCGEx::State_Completing;
			PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(CompleteWork, bInlineCompletion, { Processor->CompleteWork(); })
			FClusterProcessorBatchBase::CompleteWork();
		}

		virtual void Write() override
		{
			if (!bIsBatchValid) { return; }

			CurrentState = PCGEx::State_Writing;
			PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(Write, bInlineWrite, { Processor->Write(); })
			FClusterProcessorBatchBase::Write();
		}

		virtual void Output() override
		{
			if (!bIsBatchValid) { return; }
			for (const TSharedPtr<T>& P : Processors)
			{
				if (!P->bIsProcessorValid) { continue; }
				P->Output();
			}
		}

		virtual void Cleanup() override
		{
			FClusterProcessorBatchBase::Cleanup();
			for (const TSharedPtr<T>& P : Processors) { P->Cleanup(); }
			Processors.Empty();
		}
	};

	template <typename T>
	class TBatchWithGraphBuilder : public TBatch<T>
	{
	public:
		TBatchWithGraphBuilder(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges):
			TBatch<T>(InContext, InVtx, InEdges)
		{
			this->bRequiresGraphBuilder = true;
		}
	};

	template <typename T>
	class TBatchWithHeuristics : public TBatch<T>
	{
	public:
		TBatchWithHeuristics(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges):
			TBatch<T>(InContext, InVtx, InEdges)
		{
			this->SetRequiresHeuristics(true);
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
