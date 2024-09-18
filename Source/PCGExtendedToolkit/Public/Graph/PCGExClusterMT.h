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
		FStartClusterBatchProcessing(PCGExData::FPointIO* InPointIO, T* InTarget) : FPCGExTask(InPointIO), Target(InTarget)
		{
		}

		T* Target = nullptr;

		virtual bool ExecuteTask() override
		{
			Target->PrepareProcessing(Manager);
			return true;
		}
	};

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

		template <typename T>
		T* GetParentBatch() { return static_cast<T*>(ParentBatch); }

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
		FClusterProcessorBatchBase* ParentBatch = nullptr;

		PCGExData::FFacade* VtxDataFacade = nullptr;
		PCGExData::FFacade* EdgeDataFacade = nullptr;

		bool bAllowFetchOnEdgesDataFacade = false;

		bool bIsProcessorValid = false;

		PCGExHeuristics::THeuristicsHandler* HeuristicsHandler = nullptr;

		bool bIsTrivial = false;
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

		bool IsTrivial() const { return bIsTrivial; }

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
				PrepareLoopScopesForNodes({PCGEx::H64(0, NumNodes)});
				ProcessNodes(0, NumNodes, 0);
				OnNodesProcessingComplete();
				return;
			}

			const int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize(PerLoopIterations);

			PCGEX_ASYNC_GROUP_CHECKED(AsyncManagerPtr, ParallelLoopForNodes)
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

			PCGEX_ASYNC_GROUP_CHECKED(AsyncManagerPtr, ParallelLoopForEdges)
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

			PCGEX_ASYNC_GROUP_CHECKED(AsyncManagerPtr, ParallelLoopForRanges)
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

	class FClusterProcessorBatchBase
	{
	protected:
		PCGExMT::FTaskManager* AsyncManagerPtr = nullptr;

		const FPCGMetadataAttribute<int64>* RawLookupAttribute = nullptr;
		TArray<uint32> ReverseLookup;

		TMap<uint32, int32> EndpointsLookup;
		TArray<int32> ExpectedAdjacency;

		bool bPreparationSuccessful = false;
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

		bool PreparationSuccessful() const { return bPreparationSuccessful; }
		bool RequiresGraphBuilder() const { return bRequiresGraphBuilder; }
		bool RequiresHeuristics() const { return bRequiresHeuristics; }
		virtual void SetRequiresHeuristics(const bool bRequired = false) { bRequiresHeuristics = bRequired; }

		bool bInlineProcessing = false;
		bool bInlineCompletion = false;
		bool bInlineWrite = false;

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

		virtual void PrepareProcessing(PCGExMT::FTaskManager* AsyncManager)
		{
			AsyncManagerPtr = AsyncManager;

			VtxIO->CreateInKeys();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <= 4

			PCGExGraph::BuildEndpointsLookup(VtxIO, EndpointsLookup, ExpectedAdjacency);
			if (RequiresGraphBuilder()) { GraphBuilder = new PCGExGraph::FGraphBuilder(VtxIO, &GraphBuilderDetails, 6, EdgeCollection); }

			OnProcessingPreparationComplete();

#else
			
			const int32 NumVtx = VtxIO->GetNum();
			
			if (NumVtx < GetDefault<UPCGExGlobalSettings>()->SmallClusterSize)
			{
				// Trivial
				PCGExGraph::BuildEndpointsLookup(VtxIO, EndpointsLookup, ExpectedAdjacency);
				if (RequiresGraphBuilder()) { GraphBuilder = new PCGExGraph::FGraphBuilder(VtxIO, &GraphBuilderDetails, 6, EdgeCollection); }
				
				OnProcessingPreparationComplete();				
			}
			else
			{
				// Spread
				PCGEX_ASYNC_GROUP_CHECKED(AsyncManagerPtr, BuildEndpointLookupTask)

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
							GraphBuilder = new PCGExGraph::FGraphBuilder(VtxIO, &GraphBuilderDetails, 6, EdgeCollection);
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
			
#endif
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
		TArray<T*> TrivialProcessors;

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

			TrivialProcessors.Empty();
		}

		virtual void Process() override
		{
			FClusterProcessorBatchBase::Process();

			if (VtxIO->GetNum() <= 1) { return; }

			CurrentState = PCGExMT::State_Processing;

			for (PCGExData::FPointIO* IO : Edges)
			{
				IO->CreateInKeys();

				T* NewProcessor = new T(VtxIO, IO);
				NewProcessor->ParentBatch = this;
				NewProcessor->Context = Context;
				NewProcessor->EndpointsLookup = &EndpointsLookup;
				NewProcessor->ExpectedAdjacency = &ExpectedAdjacency;
				NewProcessor->BatchIndex = Processors.Add(NewProcessor);
				NewProcessor->VtxDataFacade = VtxDataFacade;

				if (RequiresGraphBuilder()) { NewProcessor->GraphBuilder = GraphBuilder; }
				NewProcessor->SetRequiresHeuristics(RequiresHeuristics());

				if (!PrepareSingle(NewProcessor))
				{
					Processors.Pop();
					PCGEX_DELETE(NewProcessor)
					continue;
				}

				NewProcessor->bIsTrivial = IO->GetNum() < GetDefault<UPCGExGlobalSettings>()->SmallClusterSize;
				if (NewProcessor->IsTrivial()) { TrivialProcessors.Add(NewProcessor); }
			}

			StartProcessing();
		}

		virtual void StartProcessing()
		{
			PCGEX_ASYNC_MT_LOOP_TPL(Process, bInlineProcessing, { Processor->bIsProcessorValid = Processor->Process(AsyncManagerPtr); })
		}

		virtual bool PrepareSingle(T* ClusterProcessor) { return true; }

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
			for (T* Processor : Processors)
			{
				if (!Processor->bIsProcessorValid) { continue; }
				Processor->Output();
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

	static void CompleteBatches(PCGExMT::FTaskManager* Manager, const TArrayView<FClusterProcessorBatchBase*> Batches)
	{
		for (FClusterProcessorBatchBase* Batch : Batches) { Batch->CompleteWork(); }
	}

	static void WriteBatches(PCGExMT::FTaskManager* Manager, const TArrayView<FClusterProcessorBatchBase*> Batches)
	{
		for (FClusterProcessorBatchBase* Batch : Batches) { Batch->Write(); }
	}
}
