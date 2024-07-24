// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPathfinding.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExDataForward.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExPathfindingEdges.generated.h"

class UPCGExSearchOperation;
/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExPathfindingEdgesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathfindingEdges, "Pathfinding : Edges", "Extract paths from edges clusters.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorPathfinding; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	/** Controls how goals are picked.*/
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExGoalPicker> GoalPicker;

	/** Add seed point at the beginning of the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAddSeedToPath = false;

	/** Add goal point at the beginning of the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAddGoalToPath = false;

	/** Drive how a seed selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Node Picking", meta=(PCG_Overridable))
	FPCGExNodeSelectionDetails SeedPicking;

	/** Drive how a goal selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Node Picking", meta=(PCG_Overridable))
	FPCGExNodeSelectionDetails GoalPicking;

	/** Search algorithm. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExSearchOperation> SearchAlgorithm;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExAttributeToTagDetails SeedAttributesToPathTags;

	/** Which Seed attributes to forward on paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExForwardDetails SeedForwarding;

	/** Which Goal attribute to use as tag. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExAttributeToTagDetails GoalAttributesToPathTags;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExForwardDetails GoalForwarding;

	/** Output various statistics. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Advanced")
	FPCGExPathStatistics Statistics;

	/** Whether or not to search for closest node using an octree. Depending on your dataset, enabling this may be either much faster, or slightly slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance", meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bUseOctreeSearch = false;
};


struct PCGEXTENDEDTOOLKIT_API FPCGExPathfindingEdgesContext final : public FPCGExEdgesProcessorContext
{
	friend class FPCGExPathfindingEdgesElement;

	virtual ~FPCGExPathfindingEdgesContext() override;

	PCGExData::FFacade* SeedsDataFacade = nullptr;
	PCGExData::FFacade* GoalsDataFacade = nullptr;

	PCGExData::FPointIOCollection* OutputPaths = nullptr;

	UPCGExGoalPicker* GoalPicker = nullptr;
	UPCGExSearchOperation* SearchAlgorithm = nullptr;

	FPCGExAttributeToTagDetails SeedAttributesToPathTags;
	FPCGExAttributeToTagDetails GoalAttributesToPathTags;

	PCGExData::FDataForwardHandler* SeedForwardHandler = nullptr;
	PCGExData::FDataForwardHandler* GoalForwardHandler = nullptr;

	TArray<PCGExPathfinding::FPathQuery*> PathQueries;

	void TryFindPath(
		const UPCGExSearchOperation* SearchOperation,
		const PCGExPathfinding::FPathQuery* Query,
		PCGExHeuristics::THeuristicsHandler* HeuristicsHandler);
};

class PCGEXTENDEDTOOLKIT_API FPCGExPathfindingEdgesElement final : public FPCGExEdgesProcessorElement
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

namespace PCGExPathfindingEdge
{
	class PCGEXTENDEDTOOLKIT_API FSampleClusterPathTask final : public FPCGExPathfindingTask
	{
	public:
		FSampleClusterPathTask(PCGExData::FPointIO* InPointIO,
		                       const UPCGExSearchOperation* InSearchOperation,
		                       const TArray<PCGExPathfinding::FPathQuery*>* InQueries,
		                       PCGExHeuristics::THeuristicsHandler* InHeuristics,
		                       const bool Inlined = false) :
			FPCGExPathfindingTask(InPointIO, InQueries),
			SearchOperation(InSearchOperation),
			Heuristics(InHeuristics),
			bInlined(Inlined)
		{
		}

		const UPCGExSearchOperation* SearchOperation = nullptr;
		PCGExHeuristics::THeuristicsHandler* Heuristics = nullptr;
		bool bInlined = false;

		virtual bool ExecuteTask() override;
	};

	class FProcessor final : public PCGExClusterMT::FClusterProcessor
	{
	public:
		FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
			FClusterProcessor(InVtx, InEdges)
		{
		}

		virtual ~FProcessor() override;

		UPCGExSearchOperation* SearchOperation = nullptr;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
	};
}
