// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPathfinding.h"
#include "PCGExPointsProcessor.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExPathfindingPlotEdges.generated.h"

class UPCGExHeuristicOperation;
class UPCGExSearchOperation;
/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExPathfindingPlotEdgesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathfindingPlotEdges, "Pathfinding : Plot Edges", "Extract a single path from edges clusters, going through every seed points in order.");
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UObject interface
public:
	virtual void PostInitProperties() override;

#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

public:
	/** Add seed point at the beginning of the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAddSeedToPath = true;

	/** Add goal point at the beginning of the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAddGoalToPath = true;

	/** Insert plot points inside the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAddPlotPointsToPath = true;

	/** Drive how a seed selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Node Picking", meta=(PCG_Overridable))
	FPCGExNodeSelectionSettings SeedPicking;

	/** Drive how a goal selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Node Picking", meta=(PCG_Overridable))
	FPCGExNodeSelectionSettings GoalPicking;
	
	/** Search algorithm. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Settings|Node Picking", Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExSearchOperation> SearchAlgorithm;

	/** Controls how heuristic are calculated. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExHeuristicOperation> Heuristics;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExHeuristicModifiersSettings HeuristicsModifiers;

	/** Add weight to points that are already part of the growing path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extra Weighting")
	bool bWeightUpVisited = false;

	/** Weight to add to points that are already part of the plotted path. This is a multplier of the Reference Weight.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extra Weighting", meta=(EditCondition="bWeightUpVisited"))
	double VisitedPointsWeightFactor = 1;

	/** Weight to add to edges that are already part of the plotted path. This is a multplier of the Reference Weight.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extra Weighting", meta=(EditCondition="bWeightUpVisited"))
	double VisitedEdgesWeightFactor = 1;

	/** Shares visited weight between pathfinding queries. Slow as it break parallelism. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extra Weighting", meta=(EditCondition="bWeightUpVisited"))
	bool bGlobalVisitedWeight = true;

	/** Output various statistics. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExPathStatistics Statistics;

	/** Projection settings, used by some algorithms. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionSettings ProjectionSettings;

	/** Whether or not to search for closest node using an octree. Depending on your dataset, enabling this may be either much faster, or slightly slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bUseOctreeSearch = false;
};


struct PCGEXTENDEDTOOLKIT_API FPCGExPathfindingPlotEdgesContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExPathfindingPlotEdgesElement;

	virtual ~FPCGExPathfindingPlotEdgesContext() override;

	PCGExData::FPointIOCollection* Plots = nullptr;
	PCGExData::FPointIOCollection* OutputPaths = nullptr;

	UPCGExHeuristicOperation* Heuristics = nullptr;
	UPCGExSearchOperation* SearchAlgorithm = nullptr;
	FPCGExHeuristicModifiersSettings* HeuristicsModifiers = nullptr;
	//UPCGExSubPointsBlendOperation* Blending = nullptr;

	int32 CurrentPlotIndex = -1;
	bool bWeightUpVisited = true;
	double VisitedPointsWeightFactor = 1;
	double VisitedEdgesWeightFactor = 1;
	PCGExPathfinding::FExtraWeights* GlobalExtraWeights = nullptr;

	bool bAddSeedToPath = true;
	bool bAddGoalToPath = true;
	bool bAddPlotPointsToPath = true;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPathfindingPlotEdgesElement : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};


class PCGEXTENDEDTOOLKIT_API FPCGExPlotClusterPathTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExPlotClusterPathTask(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		PCGExPathfinding::FExtraWeights* InGlobalExtraWeights = nullptr) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		GlobalExtraWeights(InGlobalExtraWeights)

	{
	}

	PCGExPathfinding::FExtraWeights* GlobalExtraWeights = nullptr;

	virtual bool ExecuteTask() override;
};
