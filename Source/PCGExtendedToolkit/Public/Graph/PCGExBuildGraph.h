// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDrawGraph.h"

#include "PCGExGraphProcessor.h"
#include "Solvers/PCGExGraphSolver.h"

#include "PCGExBuildGraph.generated.h"

constexpr PCGExMT::AsyncState State_ProbingPoints = __COUNTER__;

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExBuildGraphSettings : public UPCGExGraphProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExBuildGraphSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildGraph, "Graph : Build", "Write graph data to an attribute for each connected Graph Params. `Build Graph` uses the socket information as is.");
#endif

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** Ignores candidates weighting pass and always favors the closest one.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault))
	UPCGExGraphSolver* GraphSolver;

	virtual FName GetMainInputLabel() const override;
	virtual int32 GetPreferredChunkSize() const override;

	virtual PCGExData::EInit GetMainOutputInitMode() const override;

private:
	friend class FPCGExBuildGraphElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBuildGraphContext : public FPCGExGraphProcessorContext
{
	friend class FPCGExBuildGraphElement;
	friend class FProbeTask;

	virtual ~FPCGExBuildGraphContext() override;

	UPCGExGraphSolver* GraphSolver = nullptr;
	bool bMoveSocketOriginOnPointExtent = false;

	UPCGPointData::PointOctree* Octree = nullptr;
};


class PCGEXTENDEDTOOLKIT_API FPCGExBuildGraphElement : public FPCGExGraphProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

class PCGEXTENDEDTOOLKIT_API FProbeTask : public FPCGExNonAbandonableTask
{
public:
	FProbeTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};
