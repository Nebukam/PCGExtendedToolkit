// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExClusterBatch.h"
#include "PCGExEdgesProcessor.h"

#include "PCGExSimplifyClusters.generated.h"


namespace PCGExSimplifyClusters
{
	class FSimplifyClusterBatch;
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExSimplifyClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SimplifyClusters, "Graph : Simplify Clusters", "Simplify connections by operating on isolated chains of nodes (only two neighbors).");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorGraph; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExEdgesProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExEdgesProcessorSettings interface

	virtual FName GetVtxFilterLabel() const override;

	/** If enabled, only check for dead ends. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bOperateOnDeadEndsOnly = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bMergeAboveAngularThreshold = false;

	/** If enabled, uses an angular threshold below which nodes are merged. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bMergeAboveAngularThreshold", Units="Degrees", ClampMin=0, ClampMax=180))
	double AngularThreshold = 10;

	/** Removes hard angles instead of collinear ones. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bMergeAboveAngularThreshold"))
	bool bInvertAngularThreshold = false;

	/** If enabled, prune dead ends. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bPruneDeadEnds = false;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSimplifyClustersContext : public FPCGExEdgesProcessorContext
{
	friend class UPCGExSimplifyClustersSettings;
	friend class FPCGExSimplifyClustersElement;

	FPCGExGraphBuilderSettings GraphBuilderSettings;
	PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;

	TArray<PCGExCluster::FNodeChain*> Chains;

	virtual ~FPCGExSimplifyClustersContext() override;

	TArray<PCGExSimplifyClusters::FSimplifyClusterBatch*> Batches;

	double FixedDotThreshold = 0;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSimplifyClustersElement : public FPCGExEdgesProcessorElement
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

namespace PCGExSimplifyClusters
{
	class FClusterSimplifyProcess final : public PCGExClusterBatch::FClusterProcessingData
	{
		TArray<PCGExCluster::FNodeChain*> Chains;

	public:
		FClusterSimplifyProcess(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges);
		virtual ~FClusterSimplifyProcess() override;

		virtual bool Process(FPCGExAsyncManager* AsyncManager) override;
		virtual void CompleteWork() override;

		virtual void ProcessSingleRangeIteration(const int32 Iteration) override;

		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
	};

	class FSimplifyClusterBatch : public PCGExClusterBatch::FClusterBatchProcessingData<FClusterSimplifyProcess>
	{
	public:
		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
		FPCGExGraphBuilderSettings GraphBuilderSettings;
		PCGExData::FPointIOCollection* MainEdges = nullptr;

		FSimplifyClusterBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, TArrayView<PCGExData::FPointIO*> InEdges);
		virtual ~FSimplifyClusterBatch() override;

		virtual bool PrepareProcessing() override;
		virtual bool PrepareSingle(FClusterSimplifyProcess* ClusterProcessor) override;
	};
}
