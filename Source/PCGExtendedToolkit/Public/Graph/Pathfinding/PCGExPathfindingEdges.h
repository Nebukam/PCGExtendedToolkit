// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPathfinding.h"
#include "PCGExPathfindingProcessor.h"
#include "PCGExPointsProcessor.h"

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
	PCGEX_NODE_INFOS(PathfindingEdges, "Pathfinding : Edges", "Extract paths from edges clusters.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface
};


struct PCGEXTENDEDTOOLKIT_API FPCGExPathfindingEdgesContext : public FPCGExPathfindingProcessorContext
{
	friend class FPCGExPathfindingEdgesElement;

	virtual ~FPCGExPathfindingEdgesContext() override;

	int32 CurrentPathBufferIndex = -1;
	TArray<PCGExPathfinding::FPathQuery*> PathBuffer;
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


class PCGEXTENDEDTOOLKIT_API FSampleClusterPathTask : public FPCGExPathfindingTask
{
public:
	FSampleClusterPathTask(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO, PCGExPathfinding::FPathQuery* InQuery) :
		FPCGExPathfindingTask(InManager, InTaskIndex, InPointIO, InQuery)
	{
	}

	virtual bool ExecuteTask() override;
};
