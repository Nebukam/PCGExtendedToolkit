// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "NavigationSystem.h"
#include "PCGExPathfindingProcessor.h"

#include "PCGExPointsProcessor.h"
#include "GoalPickers/PCGExGoalPicker.h"
#include "Graph/PCGExGraph.h"
#include "Splines/SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"

#include "PCGExSampleGraphPatches.generated.h"

/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExSampleGraphPatchesSettings : public UPCGExPathfindingProcessorSettings
{
	GENERATED_BODY()

	UPCGExSampleGraphPatchesSettings(const FObjectInitializer& ObjectInitializer);

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleGraphPatches, "Sample Graph Patches", "Extract paths from graph patches.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
};


struct PCGEXTENDEDTOOLKIT_API FPCGExSampleGraphPatchesContext : public FPCGExPathfindingProcessorContext
{
	friend class FPCGExSampleGraphPatchesElement;

public:
	PCGExData::FPointIO* GoalsPoints = nullptr;
	PCGExData::FPointIOGroup* OutputPaths = nullptr;

	UPCGExGoalPicker* GoalPicker;
	UPCGExSubPointsBlendOperation* Blending;

	bool bAddSeedToPath = true;
	bool bAddGoalToPath = true;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSampleGraphPatchesElement : public FPCGExPathfindingProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;
	virtual bool Validate(FPCGContext* InContext) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

// Define the background task class
class PCGEXTENDEDTOOLKIT_API FSamplePatchPathTask : public FPCGExAsyncTask
{
public:
	FSamplePatchPathTask(
		UPCGExAsyncTaskManager* InManager, const PCGExMT::FTaskInfos& InInfos, PCGExData::FPointIO* InPointIO,
		int32 InGoalIndex, PCGExData::FPointIO* InPathPoints) :
		FPCGExAsyncTask(InManager, InInfos, InPointIO),
		GoalIndex(InGoalIndex), PathPoints(InPathPoints)
	{
	}

	int32 GoalIndex = -1;
	PCGExData::FPointIO* PathPoints;

	virtual bool ExecuteTask() override;
};
