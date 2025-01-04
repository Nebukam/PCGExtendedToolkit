// Copyright 2024 Timothé Lapetite and contributors
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
	PCGEX_CTX_STATE(MTState_ClusterProcessing)
	PCGEX_CTX_STATE(MTState_ClusterCompletingWork)
	PCGEX_CTX_STATE(MTState_ClusterWriting)

#pragma region Tasks

#define PCGEX_ASYNC_CLUSTER_PROCESSOR_LOOP(_NAME, _NUM, _PREPARE, _PROCESS, _COMPLETE, _INLINE) PCGEX_ASYNC_PROCESSOR_LOOP(_NAME, _NUM, _PREPARE, _PROCESS, _COMPLETE, _INLINE, GetClusterBatchChunkSize)

	template <typename T>
	class FStartClusterBatchProcessing final : public PCGExMT::FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FStartClusterBatchProcessing)

		FStartClusterBatchProcessing(TSharedPtr<T> InTarget,
		                             const bool bScoped)
			: FTask(),
			  Target(InTarget),
			  bScopedIndexLookupBuild(bScoped)
		{
		}

		TSharedPtr<T> Target;
		bool bScopedIndexLookupBuild = false;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override
		{
			Target->PrepareProcessing(AsyncManager, bScopedIndexLookupBuild);
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
		bool bDaisyChainProcessRange = false;

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

		virtual void RegisterConsumableAttributesWithFacade() const
		{
			// Gives an opportunity for the processor to register attributes with a valid facade
			// So selectors shortcut can be properly resolved (@Last, etc.)

			if (HeuristicsFactories)
			{
				PCGExFactories::RegisterConsumableAttributesWithFacade(*HeuristicsFactories, VtxDataFacade);
				PCGExFactories::RegisterConsumableAttributesWithFacade(*HeuristicsFactories, EdgeDataFacade);
			}
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
			PCGEX_ASYNC_CHKD(AsyncManager)

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

			// Building cluster may have taken a while so let's make sure we're still legit
			return AsyncManager->IsAvailable();
		}

#pragma region Parallel loops

		void StartParallelLoopForNodes(const int32 PerLoopIterations = -1)
		{
			PCGEX_ASYNC_CLUSTER_PROCESSOR_LOOP(
				Nodes, NumNodes,
				PrepareLoopScopesForNodes, ProcessNodes,
				OnNodesProcessingComplete,
				bInlineProcessNodes)
		}

		virtual void PrepareLoopScopesForNodes(const TArray<PCGExMT::FScope>& Loops)
		{
		}

		virtual void PrepareSingleLoopScopeForNodes(const PCGExMT::FScope& Scope)
		{
		}

		virtual void ProcessNodes(const PCGExMT::FScope& Scope)
		{
			PrepareSingleLoopScopeForNodes(Scope);
			TArray<PCGExCluster::FNode>& Nodes = *Cluster->Nodes;
			for (int i = Scope.Start; i < Scope.End; i++) { ProcessSingleNode(i, Nodes[i], Scope); }
		}

		virtual void ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const PCGExMT::FScope& Scope)
		{
		}

		virtual void OnNodesProcessingComplete()
		{
		}


		void StartParallelLoopForEdges(const int32 PerLoopIterations = -1)
		{
			PCGEX_ASYNC_CLUSTER_PROCESSOR_LOOP(
				Edges, NumEdges,
				PrepareLoopScopesForEdges, ProcessEdges,
				OnEdgesProcessingComplete,
				bInlineProcessEdges)
		}

		virtual void PrepareLoopScopesForEdges(const TArray<PCGExMT::FScope>& Loops)
		{
		}

		virtual void PrepareSingleLoopScopeForEdges(const PCGExMT::FScope& Scope)
		{
		}

		virtual void ProcessEdges(const PCGExMT::FScope& Scope)
		{
			PrepareSingleLoopScopeForEdges(Scope);
			TArray<PCGExGraph::FEdge>& ClusterEdges = *Cluster->Edges;
			for (int i = Scope.Start; i < Scope.End; i++) { ProcessSingleEdge(i, ClusterEdges[i], Scope); }
		}

		virtual void ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FEdge& Edge, const PCGExMT::FScope& Scope)
		{
		}

		virtual void OnEdgesProcessingComplete()
		{
		}

		void StartParallelLoopForRange(const int32 NumIterations, const int32 PerLoopIterations = -1)
		{
			PCGEX_ASYNC_CLUSTER_PROCESSOR_LOOP(
				Ranges, NumIterations,
				PrepareLoopScopesForRanges, ProcessRange,
				OnRangeProcessingComplete,
				bDaisyChainProcessRange)
		}

		virtual void PrepareLoopScopesForRanges(const TArray<PCGExMT::FScope>& Loops)
		{
		}

		virtual void PrepareSingleLoopScopeForRange(const PCGExMT::FScope& Scope)
		{
		}

		virtual void ProcessRange(const PCGExMT::FScope& Scope)
		{
			PrepareSingleLoopScopeForRange(Scope);
			for (int i = Scope.Start; i < Scope.End; i++) { ProcessSingleRangeIteration(i, Scope); }
		}

		virtual void ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope)
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
		TArray<TSharedRef<PCGExData::FFacade>>* EdgesDataFacades = nullptr;

		TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder;
		FPCGExGraphBuilderDetails GraphBuilderDetails;

		TArray<TSharedPtr<PCGExCluster::FCluster>> ValidClusters;

		virtual int32 GetNumProcessors() const { return -1; }

		bool PreparationSuccessful() const { return bPreparationSuccessful; }
		bool RequiresGraphBuilder() const { return bRequiresGraphBuilder; }
		bool RequiresHeuristics() const { return bRequiresHeuristics; }
		virtual void SetRequiresHeuristics(const bool bRequired) { bRequiresHeuristics = bRequired; }

		bool bDaisyChainProcessing = false;
		bool bDaisyChainCompletion = false;
		bool bDaisyChainWrite = false;

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
					GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(VtxDataFacade, &GraphBuilderDetails, 6);
					GraphBuilder->SourceEdgeFacades = EdgesDataFacades;
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
					[PCGEX_ASYNC_THIS_CAPTURE]()
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExGraph::BuildLookupTable::Complete);

						PCGEX_ASYNC_THIS

						const int32 Num = This->VtxDataFacade->GetNum();
						This->EndpointsLookup.Reserve(Num);
						for (int i = 0; i < Num; i++) { This->EndpointsLookup.Add(This->ReverseLookup[i], i); }
						This->ReverseLookup.Empty();

						if (This->RequiresGraphBuilder())
						{
							This->GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(This->VtxDataFacade, &This->GraphBuilderDetails, 6);
							This->GraphBuilder->SourceEdgeFacades = This->EdgesDataFacades;
						}

						This->OnProcessingPreparationComplete();
					};

				BuildEndpointLookupTask->OnSubLoopStartCallback =
					[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExGraph::BuildLookupTable::Range);

						PCGEX_ASYNC_THIS
						const TArray<FPCGPoint>& InKeys = This->VtxDataFacade->GetIn()->GetPoints();
						for (int i = Scope.Start; i < Scope.End; i++)
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

			VtxFacadePreloader->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE]
			{
				PCGEX_ASYNC_THIS
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
				GraphBuilder->OnCompilationEndCallback =
					[PCGEX_ASYNC_THIS_CAPTURE](const TSharedRef<PCGExGraph::FGraphBuilder>& InBuilder, const bool bSuccess)
					{
						PCGEX_ASYNC_THIS

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

		std::atomic<PCGEx::ContextState> CurrentState{PCGEx::State_InitialExecution};

		virtual int32 GetNumProcessors() const override { return Processors.Num(); }

		TBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges):
			FClusterProcessorBatchBase(InContext, InVtx, InEdges)
		{
		}

		int32 GatherValidClusters()
		{
			ValidClusters.Empty();

			for (const TSharedPtr<T>& P : Processors)
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

			PCGEX_ASYNC_CHKD_VOID(AsyncManager)

			if (VtxDataFacade->GetNum() <= 1) { return; }

			CurrentState.store(PCGEx::State_Processing, std::memory_order_release);
			TSharedPtr<FClusterProcessorBatchBase> SelfPtr = SharedThis(this);

			for (const TSharedPtr<PCGExData::FPointIO>& IO : Edges)
			{
				const TSharedPtr<T> NewProcessor = MakeShared<T>(VtxDataFacade, (*EdgesDataFacades)[IO->IOIndex]);

				NewProcessor->SetExecutionContext(ExecutionContext);
				NewProcessor->ParentBatch = SelfPtr;
				NewProcessor->NodeIndexLookup = NodeIndexLookup;
				NewProcessor->EndpointsLookup = &EndpointsLookup;
				NewProcessor->ExpectedAdjacency = &ExpectedAdjacency;
				NewProcessor->BatchIndex = Processors.Num();

				if (RequiresGraphBuilder()) { NewProcessor->GraphBuilder = GraphBuilder; }
				NewProcessor->SetRequiresHeuristics(RequiresHeuristics(), HeuristicsFactories);

				NewProcessor->RegisterConsumableAttributesWithFacade();

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
			PCGEX_ASYNC_MT_LOOP_TPL(Process, bDaisyChainProcessing, { Processor->bIsProcessorValid = Processor->Process(This->AsyncManager); })
		}

		virtual bool PrepareSingle(const TSharedPtr<T>& ClusterProcessor) { return true; }

		virtual void CompleteWork() override
		{
			if (!bIsBatchValid) { return; }

			CurrentState.store(PCGEx::State_Completing, std::memory_order_release);
			PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(CompleteWork, bDaisyChainCompletion, { Processor->CompleteWork(); })
			FClusterProcessorBatchBase::CompleteWork();
		}

		virtual void Write() override
		{
			if (!bIsBatchValid) { return; }

			CurrentState.store(PCGEx::State_Writing, std::memory_order_release);
			PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(Write, bDaisyChainWrite, { Processor->Write(); })
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
			for (const TSharedRef<T>& P : Processors) { P->Cleanup(); }
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

	static void ScheduleBatch(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const TSharedPtr<FClusterProcessorBatchBase>& Batch, const bool bScopedIndexLookupBuild)
	{
		PCGEX_LAUNCH(FStartClusterBatchProcessing<FClusterProcessorBatchBase>, Batch, bScopedIndexLookupBuild)
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
