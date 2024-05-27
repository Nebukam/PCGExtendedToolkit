// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathfinding.h"
#include "GoalPickers/PCGExGoalPicker.h"

#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExPathfindingProcessor.generated.h"

class UPCGExHeuristicOperation;
class UPCGExSearchOperation;
class UPCGExPathfindingParamsData;

/**
 * A Base node to process a set of point using GraphParams.
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExPathfindingProcessorSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathfindingProcessorSettings, "Pathfinding Processor Settings", "TOOLTIP_TEXT");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorPathfinding; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

	//~Begin UObject interface
public:
	virtual void PostInitProperties() override;

#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

	//~Begin UPCGExPathfindingProcessorSettings interface
	virtual bool GetRequiresSeeds() const;
	virtual bool GetRequiresGoals() const;

	/** Controls how goals are picked.*/
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExGoalPicker> GoalPicker;

	/** Add seed point at the beginning of the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAddSeedToPath = true;

	/** Add goal point at the beginning of the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAddGoalToPath = true;

	/** Drive how a seed selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Node Picking", meta=(PCG_Overridable))
	FPCGExNodeSelectionSettings SeedPicking;

	/** Drive how a goal selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Node Picking", meta=(PCG_Overridable))
	FPCGExNodeSelectionSettings GoalPicking;

	/** Search algorithm. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExSearchOperation> SearchAlgorithm;

	/** Controls how heuristic are calculated. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExHeuristicOperation> Heuristics;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExHeuristicModifiersSettings HeuristicsModifiers;
	//~End UPCGExPathfindingProcessorSettings interface

	/** Add weight to points that have already been visited. Slow as it break parallelism. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extra Weighting")
	bool bWeightUpVisited = false;

	/** Weight to add to points that are already part of the plotted path. This is a multplier of the Reference Weight.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extra Weighting", meta=(EditCondition="bWeightUpVisited"))
	double VisitedPointsWeightFactor = 1;

	/** Weight to add to edges that are already part of the plotted path. This is a multplier of the Reference Weight.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extra Weighting", meta=(EditCondition="bWeightUpVisited"))
	double VisitedEdgesWeightFactor = 1;

	/** Use a Seed attribute value to tag output paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	bool bUseSeedAttributeToTagPath;

	/** Which Seed attribute to use as tag. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="bUseSeedAttributeToTagPath"))
	FPCGAttributePropertyInputSelector SeedTagAttribute;

	/** Which Seed attributes to forward on paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExForwardSettings SeedForwardAttributes;

	/** Use a Goal attribute value to tag output paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	bool bUseGoalAttributeToTagPath;

	/** Which Goal attribute to use as tag. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="bUseGoalAttributeToTagPath"))
	FPCGAttributePropertyInputSelector GoalTagAttribute;

	/** Which Goal attributes to forward on paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExForwardSettings GoalForwardAttributes;

	/** Output various statistics. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Advanced")
	FPCGExPathStatistics Statistics;

	/** Projection settings, used by some algorithms. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Advanced", meta = (PCG_Overridable))
	FPCGExGeo2DProjectionSettings ProjectionSettings;

	/** Whether or not to search for closest node using an octree. Depending on your dataset, enabling this may be either much faster, or slightly slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Advanced")
	bool bUseOctreeSearch = false;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPathfindingProcessorContext : public FPCGExEdgesProcessorContext
{
	friend class UPCGExPathfindingProcessorSettings;

	virtual ~FPCGExPathfindingProcessorContext() override;

	PCGExData::FPointIO* SeedsPoints = nullptr;
	PCGExData::FPointIO* GoalsPoints = nullptr;
	PCGExData::FPointIOCollection* OutputPaths = nullptr;

	UPCGExGoalPicker* GoalPicker = nullptr;
	UPCGExSearchOperation* SearchAlgorithm = nullptr;
	UPCGExHeuristicOperation* Heuristics = nullptr;
	FPCGExHeuristicModifiersSettings* HeuristicsModifiers = nullptr;

	PCGEx::FLocalToStringGetter* SeedTagValueGetter = nullptr;
	PCGEx::FLocalToStringGetter* GoalTagValueGetter = nullptr;

	PCGExDataBlending::FDataForwardHandler* SeedForwardHandler = nullptr;
	PCGExDataBlending::FDataForwardHandler* GoalForwardHandler = nullptr;
	//UPCGExSubPointsBlendOperation* Blending = nullptr;

	bool bAddSeedToPath = true;
	bool bAddGoalToPath = true;

	PCGExPathfinding::FExtraWeights* GlobalExtraWeights = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPathfindingProcessorElement : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual FPCGContext* InitializeContext(
		FPCGExPointsProcessorContext* InContext,
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) const override;
};
