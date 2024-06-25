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

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExRefineEdgesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

	//~Begin UObject interface
public:

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(RefineEdges, "Edges : Refine", "Refine edges according to special rules.");
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExEdgeRefineOperation> Refinement;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Graph Output Settings"))
	FPCGExGraphBuilderSettings GraphBuilderSettings;

private:
	friend class FPCGExRefineEdgesElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExRefineEdgesContext final : public FPCGExEdgesProcessorContext
{
	friend class FPCGExRefineEdgesElement;

	virtual ~FPCGExRefineEdgesContext() override;

	UPCGExEdgeRefineOperation* Refinement = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExRefineEdgesElement final : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExRefineEdges
{
	class FProcessor final : public PCGExClusterMT::FClusterProcessor
	{
	protected:
		virtual PCGExCluster::FCluster* HandleCachedCluster(const PCGExCluster::FCluster* InClusterRef) override;
		mutable FRWLock NodeLock;
		mutable FRWLock EdgeLock;
		
	public:
		FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges)
			: FClusterProcessor(InVtx, InEdges)
		{
			DefaultVtxFilterValue = false;
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSingleNode(PCGExCluster::FNode& Node) override;
		virtual void ProcessSingleEdge(PCGExGraph::FIndexedEdge& Edge) override;
		virtual void CompleteWork() override;

		UPCGExEdgeRefineOperation* Refinement = nullptr;
	};

	class FProcessorBatch final : public PCGExClusterMT::TBatchWithGraphBuilder<FProcessor>
	{
	public:
		UPCGExEdgeRefineOperation* Refinement = nullptr;

		FProcessorBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, TArrayView<PCGExData::FPointIO*> InEdges);

		virtual bool PrepareSingle(FProcessor* ClusterProcessor) override;
	};
}
