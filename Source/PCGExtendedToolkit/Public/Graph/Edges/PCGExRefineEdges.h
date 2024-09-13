// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExClusterMT.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Refining/PCGExEdgeRefineOperation.h"

#include "PCGExRefineEdges.generated.h"

namespace PCGExRefineEdges
{
	class FProcessorBatch;
	const FName SourceVtxFilters = FName("VtxFilters");
	const FName SourceEdgeFilters = FName("EdgeFilters");
}

namespace PCGExHeuristics
{
	class THeuristicsHandler;
}

namespace PCGExGraph
{
	class FGraphBuilder;
	class FGraph;
}

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Refine Sanitization"))
enum class EPCGExRefineSanitization : uint8
{
	None     = 0 UMETA(DisplayName = "None", ToolTip="No sanitization."),
	Shortest = 1 UMETA(DisplayName = "Shortest", ToolTip="If a node has no edge left, restore the shortest one."),
	Longest  = 2 UMETA(DisplayName = "Longest", ToolTip="If a node has no edge left, restore the longest one."),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExRefineEdgesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

	//~Begin UObject interface
public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(RefineEdges, "Cluster : Refine", "Refine edges according to special rules.");
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExEdgeRefineOperation> Refinement;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	bool bOutputOnlyEdgesAsPoints = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExRefineSanitization Sanitization = EPCGExRefineSanitization::None;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings", EditCondition="bOutputOnlyEdgesAsPoints", EditConditionHides))
	FPCGExGraphBuilderDetails GraphBuilderDetails;

private:
	friend class FPCGExRefineEdgesElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExRefineEdgesContext final : public FPCGExEdgesProcessorContext
{
	friend class FPCGExRefineEdgesElement;

	virtual ~FPCGExRefineEdgesContext() override;

	TArray<UPCGExFilterFactoryBase*> VtxFilterFactories;
	TArray<UPCGExFilterFactoryBase*> EdgeFilterFactories;

	UPCGExEdgeRefineOperation* Refinement = nullptr;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExRefineEdgesElement final : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExRefineEdges
{
	class FProcessor final : public PCGExClusterMT::FClusterProcessor
	{
		friend class FSanitizeRangeTask;
		friend class FFilterRangeTask;

	protected:
		PCGExPointFilter::TManager* EdgeFilterManager = nullptr;
		EPCGExRefineSanitization Sanitization = EPCGExRefineSanitization::None;

		virtual PCGExCluster::FCluster* HandleCachedCluster(const PCGExCluster::FCluster* InClusterRef) override;
		mutable FRWLock NodeLock;

		TArray<bool> EdgeFilterCache;

	public:
		FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges)
			: FClusterProcessor(InVtx, InEdges)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		void StartRefinement();
		virtual void ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count) override;
		virtual void ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count) override;
		void Sanitize();
		void InsertEdges() const;
		virtual void CompleteWork() override;

		UPCGExEdgeRefineOperation* Refinement = nullptr;
	};

	class FFilterRangeTask : public PCGExMT::FPCGExTask
	{
	public:
		FFilterRangeTask(PCGExData::FPointIO* InPointIO,
		                 FProcessor* InProcessor):
			FPCGExTask(InPointIO),
			Processor(InProcessor)
		{
		}

		FProcessor* Processor = nullptr;
		uint64 Scope = 0;
		virtual bool ExecuteTask() override;
	};

	class FSanitizeRangeTask : public PCGExMT::FPCGExTask
	{
	public:
		FSanitizeRangeTask(PCGExData::FPointIO* InPointIO,
		                   FProcessor* InProcessor):
			FPCGExTask(InPointIO),
			Processor(InProcessor)
		{
		}

		FProcessor* Processor = nullptr;
		uint64 Scope = 0;
		virtual bool ExecuteTask() override;
	};

	/*
	class FProcessorBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		friend class FProcessor;
				
	public:
		FProcessorBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, TArrayView<PCGExData::FPointIO*> InEdges);
		virtual ~FProcessorBatch() override;

		virtual bool PrepareProcessing() override;
		virtual bool PrepareSingle(FProcessor* ClusterProcessor) override;
		//virtual void CompleteWork() override;
		virtual void Write() override;
	};
	*/
}
