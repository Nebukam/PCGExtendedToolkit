// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPathfinding.h"
#include "PCGExPathfindingProcessor.h"
#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"

#include "PCGExPathfindingPlotEdges.generated.h"

/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExPathfindingPlotEdgesSettings : public UPCGExPathfindingProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPathfindingPlotEdgesSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathfindingPlotEdges, "Pathfinding : Plot Edges", "Extract a single path from edges islands, going through every seed points in order.");
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual bool GetRequiresGoals() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface
};


struct PCGEXTENDEDTOOLKIT_API FPCGExPathfindingPlotEdgesContext : public FPCGExPathfindingProcessorContext
{
	friend class FPCGExPathfindingPlotEdgesElement;

	virtual ~FPCGExPathfindingPlotEdgesContext() override;

	mutable FRWLock BufferLock;

	TArray<PCGExPathfinding::FPathQuery*> PathBuffer;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPathfindingPlotEdgesElement : public FPCGExPathfindingProcessorElement
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
class PCGEXTENDEDTOOLKIT_API FSampleMeshPathTask : public FPCGExPathfindingTask
{
public:
	FSampleMeshPathTask(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO, PCGExPathfinding::FPathQuery* InQuery) :
		FPCGExPathfindingTask(InManager, InTaskIndex, InPointIO, InQuery)
	{
	}

	virtual bool ExecuteTask() override;
};
