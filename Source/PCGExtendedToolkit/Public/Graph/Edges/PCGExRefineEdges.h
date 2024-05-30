// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Refining/PCGExEdgeRefineOperation.h"
#include "PCGExRefineEdges.generated.h"

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
	virtual void PostInitProperties() override;

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

struct PCGEXTENDEDTOOLKIT_API FPCGExRefineEdgesContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExRefineEdgesElement;

	virtual ~FPCGExRefineEdgesContext() override;

	PCGExHeuristics::THeuristicsHandler* HeuristicsHandler = nullptr;

	UPCGExEdgeRefineOperation* Refinement;

	FPCGExGraphBuilderSettings GraphBuilderSettings;
	PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExRefineEdgesElement : public FPCGExEdgesProcessorElement
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


class PCGEXTENDEDTOOLKIT_API FPCGExRefineEdgesTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExRefineEdgesTask(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		PCGExCluster::FCluster* InCluster,
		PCGExData::FPointIO* InEdgeIO) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		Cluster(InCluster),
		EdgeIO(InEdgeIO)
	{
	}

	PCGExCluster::FCluster* Cluster = nullptr;
	PCGExData::FPointIO* EdgeIO = nullptr;

	virtual bool ExecuteTask() override;
};
