// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExClusterMT.h"
#include "PCGExEdgesProcessor.h"

#include "PCGExSimplifyClusters.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSimplifyClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SimplifyClusters, "Cluster : Simplify", "Simplify connections by operating on isolated chains of nodes (only two neighbors).");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorCluster; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExEdgesProcessorSettings interface	
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourceKeepConditionLabel, "Prevents vtx from being pruned by the simplification process", PCGExFactories::ClusterNodeFilters, false)
	//~End UPCGExEdgesProcessorSettings interface

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

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSimplifyClustersContext : public FPCGExEdgesProcessorContext
{
	friend class UPCGExSimplifyClustersSettings;
	friend class FPCGExSimplifyClustersElement;

	virtual ~FPCGExSimplifyClustersContext() override;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSimplifyClustersElement final : public FPCGExEdgesProcessorElement
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

namespace PCGExSimplifyClusters
{
	class FProcessor final : public PCGExClusterMT::FClusterProcessor
	{
		TArray<bool> Breakpoints;

		TArray<PCGExCluster::FNodeChain*> Chains;

		FPCGExSimplifyClustersContext* LocalTypedContext = nullptr;
		const UPCGExSimplifyClustersSettings* LocalSettings = nullptr;

	public:
		FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
			FClusterProcessor(InVtx, InEdges)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void CompleteWork() override;

		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 Count) override;

		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
	};
}
