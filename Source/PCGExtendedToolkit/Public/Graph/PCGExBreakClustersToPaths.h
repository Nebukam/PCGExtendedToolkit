// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExChain.h"


#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExBreakClustersToPaths.generated.h"

UENUM()
enum class EPCGExBreakClusterOperationTarget : uint8
{
	Paths = 0 UMETA(DisplayName = "Paths", ToolTip="Operate on edge chains which form paths with no crossings.  e.g, nodes with only two neighbors."),
	Edges = 1 UMETA(DisplayName = "Edges", ToolTip="Operate on each edge individually (very expensive)"),
};

UENUM()
enum class EPCGExBreakClusterLeavesHandling : uint8
{
	Include = 0 UMETA(DisplayName = "Include Leaves", ToolTip="Include leaves."),
	Exclude = 1 UMETA(DisplayName = "Exclude Leaves", ToolTip="Exclude leaves."),
	Only    = 2 UMETA(DisplayName = "Only Leaves", ToolTip="Only process leaves."),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
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
	virtual bool SupportsEdgeSorting() const override { return DirectionSettings.RequiresSortingRules(); }
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;
	PCGEX_NODE_POINT_FILTER(FName("Break Conditions"), "Filters used to know which points are 'break' points.", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings

	/** How to handle leaves */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExBreakClusterLeavesHandling LeavesHandling = EPCGExBreakClusterLeavesHandling::Include;

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

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bTagIfClosedLoop = true;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, EditCondition="bTagIfClosedLoop"))
	FString IsClosedLoopTag = TEXT("ClosedLoop");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bTagIfOpenPath = false;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, EditCondition="bTagIfOpenPath"))
	FString IsOpenPathTag = TEXT("OpenPath");

private:
	friend class FPCGExBreakClustersToPathsElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBreakClustersToPathsContext final : FPCGExEdgesProcessorContext
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
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExBreakClustersToPathsContext, UPCGExBreakClustersToPathsSettings>
	{
		friend class FBatch;

	protected:
		TSharedPtr<TArray<int8>> Breakpoints;
		TSharedPtr<PCGExCluster::FNodeChainBuilder> ChainBuilder;

		FPCGExEdgeDirectionSettings DirectionSettings;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void CompleteWork() override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 Count) override;
		virtual void ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FEdge& Edge, const int32 LoopIdx, const int32 Count) override;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		friend class FProcessor;

	protected:
		FPCGExEdgeDirectionSettings DirectionSettings;
		TSharedPtr<TArray<int8>> Breakpoints;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges):
			TBatch(InContext, InVtx, InEdges)
		{
			bAllowVtxDataFacadeScopedGet = true;
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void Process() override;
		virtual bool PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor) override;
		virtual void OnProcessingPreparationComplete() override;
	};
}
