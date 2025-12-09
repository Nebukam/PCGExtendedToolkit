// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGExVersion.h"
#include "CoreMinimal.h"
#include "PCGExContext.h"
#include "PCGExEdgeDirectionSettings.h"
#if PCGEX_ENGINE_VERSION > 506
#include "PCGPointPropertiesTraits.h"
#else
#include "PCGCommon.h"
#endif
#include "Details/PCGExDetailsGraph.h"
#include "Geometry/PCGExGeo.h"

namespace PCGExMT
{
	class FTaskManager;
}

template <typename T>
class FPCGMetadataAttribute;

class UPCGSettings;
struct FPCGExContext;

namespace PCGExGraph
{
	struct FGraphMetadataDetails;
	class FGraphBuilder;
}

namespace PCGEx
{
	class FWorkHandle;
}

namespace PCGExCluster
{
	struct FNode;
}

class UPCGExPointFilterFactoryData;

namespace PCGExHeuristics
{
	class FHeuristicsHandler;
}

class UPCGExHeuristicsFactoryData;

namespace PCGExClusterFilter
{
	class FManager;
}

namespace PCGExClusterMT
{
	PCGEX_CTX_STATE(MTState_ClusterProcessing)
	PCGEX_CTX_STATE(MTState_ClusterCompletingWork)
	PCGEX_CTX_STATE(MTState_ClusterWriting)

	class IBatch;

	class PCGEXTENDEDTOOLKIT_API IProcessor : public TSharedFromThis<IProcessor>
	{
		friend class IBatch;

	protected:
		FPCGExContext* ExecutionContext = nullptr;
		UPCGSettings* ExecutionSettings = nullptr;

		TWeakPtr<PCGEx::FWorkHandle> WorkHandle;
		TSharedPtr<PCGExMT::FTaskManager> AsyncManager;

		const TArray<TObjectPtr<const UPCGExHeuristicsFactoryData>>* HeuristicsFactories = nullptr;
		FPCGExEdgeDirectionSettings DirectionSettings;

		bool bWantsProjection = false;
		FPCGExGeo2DProjectionDetails ProjectionDetails;
		TSharedPtr<TArray<FVector2D>> ProjectedVtxPositions;

		bool bBuildCluster = true;
		bool bWantsHeuristics = false;

		bool bForceSingleThreadedProcessNodes = false;
		bool bForceSingleThreadedProcessEdges = false;
		bool bForceSingleThreadedProcessRange = false;

		int32 NumNodes = 0;
		int32 NumEdges = 0;

		virtual TSharedPtr<PCGExCluster::FCluster> HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef);

		void ForwardCluster() const;

	public:
		TSharedRef<PCGExData::FFacade> VtxDataFacade;
		TSharedRef<PCGExData::FFacade> EdgeDataFacade;

		TSharedPtr<PCGEx::FIndexLookup> NodeIndexLookup;

		TWeakPtr<IBatch> ParentBatch;

		template <typename T>
		T* GetParentBatch() { return static_cast<T*>(ParentBatch.Pin().Get()); }

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

		IProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade);

		virtual void SetExecutionContext(FPCGExContext* InContext);

		void SetProjectionDetails(const FPCGExGeo2DProjectionDetails& InDetails, const TSharedPtr<TArray<FVector2D>>& InProjectedVtxPositions, const bool InWantsProjection);

		virtual ~IProcessor() = default;

		virtual void RegisterConsumableAttributesWithFacade() const;

		virtual bool IsTrivial() const { return bIsTrivial; }

		void SetWantsHeuristics(const bool bRequired, const TArray<TObjectPtr<const UPCGExHeuristicsFactoryData>>* InHeuristicsFactories);

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager);

#pragma region Parallel loops

		void StartParallelLoopForNodes(const int32 PerLoopIterations = -1);
		virtual void PrepareLoopScopesForNodes(const TArray<PCGExMT::FScope>& Loops);
		virtual void ProcessNodes(const PCGExMT::FScope& Scope);
		virtual void OnNodesProcessingComplete();

		void StartParallelLoopForEdges(const int32 PerLoopIterations = -1);
		virtual void PrepareLoopScopesForEdges(const TArray<PCGExMT::FScope>& Loops);
		virtual void ProcessEdges(const PCGExMT::FScope& Scope);
		virtual void OnEdgesProcessingComplete();

		void StartParallelLoopForRange(const int32 NumIterations, const int32 PerLoopIterations = -1);
		virtual void PrepareLoopScopesForRanges(const TArray<PCGExMT::FScope>& Loops);
		virtual void ProcessRange(const PCGExMT::FScope& Scope);
		virtual void OnRangeProcessingComplete();

#pragma endregion

		virtual void CompleteWork();
		virtual void Write();
		virtual void Output();
		virtual void Cleanup();

		const TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>* VtxFilterFactories = nullptr;
		TSharedPtr<TArray<int8>> VtxFilterCache;

		const TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>* EdgeFilterFactories = nullptr;
		TArray<int8> EdgeFilterCache;

	protected:
		TSharedPtr<PCGExClusterFilter::FManager> VtxFiltersManager;
		virtual bool InitVtxFilters(const TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>* InFilterFactories);
		virtual void FilterVtxScope(const PCGExMT::FScope& Scope);

		bool IsNodePassingFilters(const PCGExCluster::FNode& Node) const;

		bool DefaultEdgeFilterValue = true;
		TSharedPtr<PCGExClusterFilter::FManager> EdgesFiltersManager;
		virtual bool InitEdgesFilters(const TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>* InFilterFactories);
		virtual void FilterEdgeScope(const PCGExMT::FScope& Scope);
	};

	template <typename TContext, typename TSettings>
	class TProcessor : public IProcessor
	{
	protected:
		TContext* Context = nullptr;
		const TSettings* Settings = nullptr;

	public:
		TProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: IProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual void SetExecutionContext(FPCGExContext* InContext) override
		{
			IProcessor::SetExecutionContext(InContext);
			Context = static_cast<TContext*>(ExecutionContext);
			Settings = InContext->GetInputSettings<TSettings>();
		}

		TContext* GetContext() { return Context; }
		const TSettings* GetSettings() { return Settings; }
	};

	class PCGEXTENDEDTOOLKIT_API IBatch : public TSharedFromThis<IBatch>
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
		bool bWantsHeuristics = false;
		bool bRequiresGraphBuilder = false;

		bool bWantsProjection = false;
		bool bWantsPerClusterProjection = false;
		FPCGExGeo2DProjectionDetails ProjectionDetails;
		TSharedPtr<TArray<FVector2D>> ProjectedVtxPositions;

		virtual TSharedPtr<IProcessor> NewProcessorInstance(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade) const;

	public:
		TArray<TSharedRef<IProcessor>> Processors;
		std::atomic<PCGExCommon::ContextState> CurrentState{PCGExCommon::State_InitialExecution};
		int32 GetNumProcessors() const { return Processors.Num(); }

		bool bIsBatchValid = true;
		FPCGExContext* ExecutionContext = nullptr;
		UPCGSettings* ExecutionSettings = nullptr;

		TWeakPtr<PCGEx::FWorkHandle> WorkHandle;
		const TArray<TObjectPtr<const UPCGExHeuristicsFactoryData>>* HeuristicsFactories = nullptr;

		const TSharedRef<PCGExData::FFacade> VtxDataFacade;
		bool bAllowVtxDataFacadeScopedGet = false;

		bool bSkipCompletion = false;
		bool bRequiresWriteStep = false;
		bool bWriteVtxDataFacade = false;
		EPCGPointNativeProperties AllocateVtxProperties = EPCGPointNativeProperties::None;

		TArray<TSharedPtr<PCGExData::FPointIO>> Edges;
		TArray<TSharedRef<PCGExData::FFacade>>* EdgesDataFacades = nullptr;
		TWeakPtr<PCGExData::FPointIOCollection> GraphEdgeOutputCollection;

		TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder;
		FPCGExGraphBuilderDetails GraphBuilderDetails;

		TArray<TSharedPtr<PCGExCluster::FCluster>> ValidClusters;

		const TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>* VtxFilterFactories = nullptr;
		const TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>* EdgeFilterFactories = nullptr;
		bool DefaultVtxFilterValue = true;
		TSharedPtr<TArray<int8>> VtxFilterCache;

		bool PreparationSuccessful() const { return bPreparationSuccessful; }
		bool RequiresGraphBuilder() const { return bRequiresGraphBuilder; }
		bool WantsHeuristics() const { return bWantsHeuristics; }
		virtual void SetWantsHeuristics(const bool bRequired) { bWantsHeuristics = bRequired; }

		bool bForceSingleThreadedProcessing = false;
		bool bForceSingleThreadedCompletion = false;
		bool bForceSingleThreadedWrite = false;

		IBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);

		virtual ~IBatch() = default;

		virtual void SetExecutionContext(FPCGExContext* InContext);

		template <typename T>
		T* GetContext() { return static_cast<T*>(ExecutionContext); }

		template <typename T>
		TSharedPtr<T> GetProcessor(const int32 Index) { return StaticCastSharedPtr<T>(Processors[Index].ToSharedPtr()); }

		template <typename T>
		TSharedRef<T> GetProcessorRef(const int32 Index) { return StaticCastSharedRef<T>(Processors[Index]); }

		bool WantsProjection() const { return bWantsProjection; }
		bool WantsPerClusterProjection() const { return bWantsPerClusterProjection; }
		virtual void SetProjectionDetails(const FPCGExGeo2DProjectionDetails& InProjectionDetails);

		virtual void PrepareProcessing(const TSharedPtr<PCGExMT::FTaskManager> AsyncManagerPtr, const bool bScopedIndexLookupBuild);
		virtual bool PrepareSingle(const TSharedPtr<IProcessor>& InProcessor);
		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader);
		virtual void OnProcessingPreparationComplete();

		virtual void Process();
		virtual void StartProcessing();

	protected:
		virtual void OnInitialPostProcess();

	public:
		int32 GatherValidClusters();

		virtual void CompleteWork();
		virtual void Write();

		virtual const PCGExGraph::FGraphMetadataDetails* GetGraphMetadataDetails();

		virtual void CompileGraphBuilder(const bool bOutputToContext);

		virtual void Output();
		virtual void Cleanup();

	protected:
		virtual void AllocateVtxPoints();
	};

	template <typename T>
	class TBatch : public IBatch
	{
	protected:
		virtual TSharedPtr<IProcessor> NewProcessorInstance(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade) const override
		{
			TSharedPtr<IProcessor> NewInstance = MakeShared<T>(InVtxDataFacade, InEdgeDataFacade);
			return NewInstance;
		}

	public:
		TBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
			: IBatch(InContext, InVtx, InEdges)
		{
		}
	};

	PCGEXTENDEDTOOLKIT_API void ScheduleBatch(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const TSharedPtr<IBatch>& Batch, const bool bScopedIndexLookupBuild);
}
