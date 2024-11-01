// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathfinding.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExDataForward.h"
#include "AI/Navigation/NavigationTypes.h"


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"
#include "PCGExPathfindingNavmesh.generated.h"

class UPCGExSubPointsBlendOperation;
class UPCGExGoalPicker;


UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Pathfinding Navmesh Mode")--E*/)
enum class EPCGExPathfindingNavmeshMode : uint8
{
	Regular      = 0 UMETA(DisplayName = "Regular", ToolTip="Regular pathfinding"),
	Hierarchical = 1 UMETA(DisplayName = "HIerarchical", ToolTip="Cell-based pathfinding"),
};

/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPathfindingNavmeshSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathfindingNavmesh, "Pathfinding : Navmesh", "Extract paths from navmesh.");
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

	//~Begin UPCGExPointsProcessorSettings
	virtual PCGExData::EInit GetMainOutputInitMode() const override;

	virtual FName GetMainInputLabel() const override { return PCGExGraph::SourceSeedsLabel; }
	virtual FName GetMainOutputLabel() const override { return PCGExGraph::OutputPathsLabel; }
	//~End UPCGExPointsProcessorSettings


	/** Controls how goals are picked.*/
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExGoalPicker> GoalPicker;

	/** Add seed point at the beginning of the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAddSeedToPath = true;

	/** Add goal point at the beginning of the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAddGoalToPath = true;

	/** Whether the pathfinding requires a naviguable end location. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bRequireNavigableEndLocation = true;

	/** Fuse sub points by distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ClampMin=0.001))
	double FuseDistance = 10;

	/** Controls how path points blend from seed to goal. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Settings|Blending", Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExSubPointsBlendOperation> Blending;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExAttributeToTagDetails SeedAttributesToPathTags;

	/** Which Seed attributes to forward on paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExForwardDetails SeedForwarding;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExAttributeToTagDetails GoalAttributesToPathTags;

	/** Which Goal attributes to forward on paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExForwardDetails GoalForwarding;


	/** Pathfinding mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Advanced")
	EPCGExPathfindingNavmeshMode PathfindingMode = EPCGExPathfindingNavmeshMode::Regular;

	/** Nav agent to be used by the nav system. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Advanced")
	FNavAgentProperties NavAgentProperties;
};


struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathfindingNavmeshContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExPathfindingNavmeshElement;

	TSharedPtr<PCGExData::FFacade> SeedsDataFacade;
	TSharedPtr<PCGExData::FFacade> GoalsDataFacade;

	TSharedPtr<PCGExData::FPointIOCollection> OutputPaths;

	UPCGExGoalPicker* GoalPicker = nullptr;
	UPCGExSubPointsBlendOperation* Blending = nullptr;

	TArray<PCGExPathfinding::FSeedGoalPair> PathQueries;

	FNavAgentProperties NavAgentProperties;

	bool bRequireNavigableEndLocation = true;
	EPCGExPathfindingNavmeshMode PathfindingMode;
	double FuseDistance = 10;

	FPCGExAttributeToTagDetails SeedAttributesToPathTags;
	FPCGExAttributeToTagDetails GoalAttributesToPathTags;

	TSharedPtr<PCGExData::FDataForwardHandler> SeedForwardHandler;
	TSharedPtr<PCGExData::FDataForwardHandler> GoalForwardHandler;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathfindingNavmeshElement final : public FPCGExPointsProcessorElement
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


class /*PCGEXTENDEDTOOLKIT_API*/ FSampleNavmeshTask final : public FPCGExPathfindingTask
{
public:
	FSampleNavmeshTask(
		const TSharedPtr<PCGExData::FPointIO>& InPointIO, const TArray<PCGExPathfinding::FSeedGoalPair>* InQueries) :
		FPCGExPathfindingTask(InPointIO, InQueries)
	{
	}

	virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
};
