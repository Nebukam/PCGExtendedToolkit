// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathfinding.h"
#include "PCGExPointsProcessor.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"
#include "PCGExPathfindingPlotNavmesh.generated.h"

/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExPathfindingPlotNavmeshSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPathfindingPlotNavmeshSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PCGExPathfindingPlotNavmesh, "Pathfinding : Plot Navmesh", "Extract a single paths from navmesh, going through each seed points in order.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorPathfinding; }
#endif

	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;

	virtual FName GetMainInputLabel() const override;
	virtual FName GetMainOutputLabel() const override;

public:
	/** Add seed point at the beginning of the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAddSeedToPath = true;

	/** Add goal point at the end of the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAddGoalToPath = true;

	/** Insert plot points inside the path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAddPlotPointsToPath = false;

	/** Whether the pathfinding requires a naviguable end location. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bRequireNavigableEndLocation = true;

	/** Fuse sub points by distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	double FuseDistance = 10;

	/** Controls how path points blend from seed to goal. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExSubPointsBlendOperation> Blending;

	/** Pathfinding mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExPathfindingNavmeshMode PathfindingMode = EPCGExPathfindingNavmeshMode::Regular;

	/** Nav agent to be used by the nav system. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FNavAgentProperties NavAgentProperties;

	/** If left empty, will attempt to fetch the default nav data instance.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	ANavigationData* NavData = nullptr;
};


struct PCGEXTENDEDTOOLKIT_API FPCGExPathfindingPlotNavmeshContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExPathfindingPlotNavmeshElement;

	virtual ~FPCGExPathfindingPlotNavmeshContext() override;

	PCGExData::FPointIOGroup* OutputPaths = nullptr;
	UPCGExSubPointsBlendOperation* Blending = nullptr;

	bool bAddSeedToPath = true;
	bool bAddGoalToPath = true;
	bool bAddPlotPointsToPath = true;

	FNavAgentProperties NavAgentProperties;

	ANavigationData* NavData = nullptr;

	bool bRequireNavigableEndLocation = true;
	EPCGExPathfindingNavmeshMode PathfindingMode;
	double FuseDistance = 10;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPathfindingPlotNavmeshElement : public FPCGExPointsProcessorElementBase
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

// Define the background task class
class PCGEXTENDEDTOOLKIT_API FPlotNavmeshTask : public FPCGExNonAbandonableTask
{
public:
	FPlotNavmeshTask(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};
