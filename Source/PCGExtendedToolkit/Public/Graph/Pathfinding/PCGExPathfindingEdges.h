﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPathfinding.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExDataForward.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Paths/PCGExPaths.h"

#include "PCGExPathfindingEdges.generated.h"

class UPCGExSearchInstancedFactory;
/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="pathfinding/pathfinding-edges"))
class UPCGExPathfindingEdgesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathfindingEdges, "Pathfinding : Edges", "Extract paths from edges clusters.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorPathfinding; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	//~Begin UObject interface
#if WITH_EDITOR
	virtual void PostInitProperties() override;
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

	/** What are the paths made of. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPathComposition PathComposition = EPCGExPathComposition::Vtx;

	/** Drive how a seed selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Node Picking", meta=(PCG_Overridable))
	FPCGExNodeSelectionDetails SeedPicking;

	/** Drive how a goal selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Node Picking", meta=(PCG_Overridable))
	FPCGExNodeSelectionDetails GoalPicking;

	/** Search algorithm. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExSearchInstancedFactory> SearchAlgorithm;

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

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName="Paths Output Settings"))
	FPCGExPathOutputDetails PathOutputDetails;

	/** Whether or not to search for closest node using an octree. Depending on your dataset, enabling this may be either much faster, or slightly slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bUseOctreeSearch = false;
};

struct FPCGExPathfindingEdgesContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExPathfindingEdgesElement;

	TSharedPtr<PCGExData::FFacade> SeedsDataFacade;
	TSharedPtr<PCGExData::FFacade> GoalsDataFacade;

	TSharedPtr<PCGExData::FPointIOCollection> OutputPaths;

	UPCGExGoalPicker* GoalPicker = nullptr;
	UPCGExSearchInstancedFactory* SearchAlgorithm = nullptr;

	FPCGExAttributeToTagDetails SeedAttributesToPathTags;
	FPCGExAttributeToTagDetails GoalAttributesToPathTags;

	TSharedPtr<PCGExData::FDataForwardHandler> SeedForwardHandler;
	TSharedPtr<PCGExData::FDataForwardHandler> GoalForwardHandler;

	TArray<uint64> SeedGoalPairs;

	void BuildPath(const TSharedPtr<PCGExPathfinding::FPathQuery>& Query);

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExPathfindingEdgesElement final : public FPCGExEdgesProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(PathfindingEdges)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExPathfindingEdges
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExPathfindingEdgesContext, UPCGExPathfindingEdgesSettings>
	{
		TArray<TSharedPtr<PCGExPathfinding::FPathQuery>> Queries;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;


		TSharedPtr<FPCGExSearchOperation> SearchOperation;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
	};
}
