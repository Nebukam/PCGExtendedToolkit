// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPathfinding.h"
#include "PCGExPointsProcessor.h"


#include "Graph/PCGExEdgesProcessor.h"
#include "Heuristics/PCGExHeuristics.h"

#include "PCGExPathfindingPlotEdges.generated.h"

class UPCGExHeuristicOperation;
class UPCGExSearchOperation;
/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPathfindingPlotEdgesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathfindingPlotEdges, "Pathfinding : Plot Edges", "Extract a single path from edges clusters, going through every seed points in order.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorPathfinding; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UObject interface
public:
#if WITH_EDITOR

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	/** Add seed point at the beginning of the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAddSeedToPath = false;

	/** Add goal point at the beginning of the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAddGoalToPath = false;

	/** Insert plot points inside the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAddPlotPointsToPath = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bClosedLoop = false;

	/** Drive how a seed selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Node Picking", meta=(PCG_Overridable))
	FPCGExNodeSelectionDetails SeedPicking;

	/** Drive how a goal selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Node Picking", meta=(PCG_Overridable))
	FPCGExNodeSelectionDetails GoalPicking;

	/** Search algorithm. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExSearchOperation> SearchAlgorithm;

	/** Output various statistics. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExPathStatistics Statistics;

	/** Whether or not to search for closest node using an octree. Depending on your dataset, enabling this may be either much faster, or much slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance", meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bUseOctreeSearch = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bOmitCompletePathOnFailedPlot = false;
};


struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathfindingPlotEdgesContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExPathfindingPlotEdgesElement;

	TSharedPtr<PCGExData::FPointIOCollection> Plots;
	TSharedPtr<PCGExData::FPointIOCollection> OutputPaths;

	UPCGExSearchOperation* SearchAlgorithm = nullptr;

	void TryFindPath(
		const UPCGExSearchOperation* SearchOperation,
		const TSharedPtr<PCGExData::FPointIO>& InPlotPoints, const TSharedPtr<PCGExHeuristics::THeuristicsHandler>& HeuristicsHandler) const;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathfindingPlotEdgesElement final : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExPathfindingPlotEdge
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPlotClusterPathTask final : public FPCGExPathfindingTask
	{
	public:
		FPCGExPlotClusterPathTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		                          const UPCGExSearchOperation* InSearchOperation,
		                          const TSharedPtr<PCGExData::FPointIOCollection>& InPlots,
		                          const TSharedPtr<PCGExHeuristics::THeuristicsHandler>& InHeuristics,
		                          const bool Inlined = false) :
			FPCGExPathfindingTask(InPointIO, nullptr),
			SearchOperation(InSearchOperation),
			Plots(InPlots),
			Heuristics(InHeuristics),
			bInlined(Inlined)
		{
		}

		const UPCGExSearchOperation* SearchOperation = nullptr;
		TSharedPtr<PCGExData::FPointIOCollection> Plots;
		TSharedPtr<PCGExHeuristics::THeuristicsHandler> Heuristics;
		bool bInlined = false;

		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};

	class FProcessor final : public PCGExClusterMT::TClusterProcessor<FPCGExPathfindingPlotEdgesContext, UPCGExPathfindingPlotEdgesSettings>
	{
	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			TClusterProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		UPCGExSearchOperation* SearchOperation = nullptr;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
	};
}
