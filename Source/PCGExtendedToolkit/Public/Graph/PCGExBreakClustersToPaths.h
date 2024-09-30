// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"


#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExBreakClustersToPaths.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Break Cluster Operation Target"))
enum class EPCGExBreakClusterOperationTarget : uint8
{
	Paths = 0 UMETA(DisplayName = "Paths", ToolTip="Operate on edge chains which form paths with no crossings.  e.g, nodes with only two neighbors."),
	Edges = 1 UMETA(DisplayName = "Edges", ToolTip="Operate on each edge individually (very expensive)"),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBreakClustersToPathsSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BreakClustersToPaths, "Cluster : Break to Paths", "Create individual paths from continuous edge chains.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorCluster; }
#endif

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	PCGEX_NODE_POINT_FILTER(FName("Break Conditions"), "Filters used to know which points are 'break' points.", PCGExFactories::ClusterNodeFilters, false)
	//~End UPCGExPointsProcessorSettings

	/** Operation target mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExBreakClusterOperationTarget OperateOn;

	/** Defines the direction in which points will be ordered to form the final paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExEdgeDirectionSettings DirectionSettings;

	/** Do not output paths that have less points that this value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=2))
	int32 MinPointCount = 2;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bOmitAbovePointCount = false;

	/** Do not output paths that have more points that this value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bOmitAbovePointCount", ClampMin=2))
	int32 MaxPointCount = 500;

private:
	friend class FPCGExBreakClustersToPathsElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBreakClustersToPathsContext final : public FPCGExEdgesProcessorContext
{
	friend class FPCGExBreakClustersToPathsElement;

	TSharedPtr<PCGExData::FPointIOCollection> Paths;
	TArray<TSharedPtr<PCGExCluster::FNodeChain>> Chains;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBreakClustersToPathsElement final : public FPCGExEdgesProcessorElement
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

namespace PCGExBreakClustersToPaths
{
	class FProcessor final : public PCGExClusterMT::TClusterProcessor<FPCGExBreakClustersToPathsContext, UPCGExBreakClustersToPathsSettings>
	{
		TArray<bool> Breakpoints;

		TArray<TSharedPtr<PCGExCluster::FNodeChain>> Chains;
		FPCGExEdgeDirectionSettings DirectionSettings;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			TClusterProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
			bCacheVtxPointIndices = true;
		}

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void CompleteWork() override;

		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 Count) override;
		virtual void ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count) override;
	};

	class FProcessorBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		friend class FProcessor;

		FPCGExEdgeDirectionSettings DirectionSettings;

	public:
		FProcessorBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges):
			TBatch<FProcessor>(InContext, InVtx, InEdges)
		{
		}

		virtual void OnProcessingPreparationComplete() override;
	};
}
