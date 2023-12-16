// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "NavigationSystem.h"
#include "PCGExPathfindingProcessor.h"

#include "PCGExPointsProcessor.h"
#include "GoalPickers/PCGExGoalPicker.h"
#include "Graph/PCGExGraph.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"

#include "PCGExPathfindingEdges.generated.h"

/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExPathfindingEdgesSettings : public UPCGExPathfindingProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPathfindingEdgesSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathfindingEdges, "Pathfinding : Edges", "Extract paths from edges islands.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
};


struct PCGEXTENDEDTOOLKIT_API FPCGExPathfindingEdgesContext : public FPCGExPathfindingProcessorContext
{
	friend class FPCGExPathfindingEdgesElement;

	virtual ~FPCGExPathfindingEdgesContext() override;

};

class PCGEXTENDEDTOOLKIT_API FPCGExPathfindingEdgesElement : public FPCGExPathfindingProcessorElement
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
class PCGEXTENDEDTOOLKIT_API FSampleIslandPathTask : public FPCGExNonAbandonableTask
{
public:
	FSampleIslandPathTask(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		int32 InGoalIndex, PCGExData::FPointIO* InPathPoints) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		GoalIndex(InGoalIndex), PathPoints(InPathPoints)
	{
	}

	int32 GoalIndex = -1;
	PCGExData::FPointIO* PathPoints;

	virtual bool ExecuteTask() override;
};
