// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDrawCustomGraph.h"

#include "PCGExCustomGraphProcessor.h"
#include "Solvers/PCGExCustomGraphSolver.h"

#include "PCGExBuildCustomGraph.generated.h"

constexpr PCGExMT::AsyncState State_ProbingPoints = __COUNTER__;

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExBuildCustomGraphSettings : public UPCGExCustomGraphProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExBuildCustomGraphSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildCustomGraph, "Custom Graph : Build", "Write graph data to an attribute for each connected Graph Params. `Build Graph` uses the socket information as is.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual FName GetMainInputLabel() const override;
	virtual int32 GetPreferredChunkSize() const override;

	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Ignores candidates weighting pass and always favors the closest one.*/
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault))
	UPCGExCustomGraphSolver* GraphSolver;

private:
	friend class FPCGExBuildCustomGraphElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBuildCustomGraphContext : public FPCGExCustomGraphProcessorContext
{
	friend class FPCGExBuildCustomGraphElement;
	friend class FProbeTask;

	virtual ~FPCGExBuildCustomGraphContext() override;

	UPCGExCustomGraphSolver* GraphSolver = nullptr;
	bool bMoveSocketOriginOnPointExtent = false;

	UPCGPointData::PointOctree* Octree = nullptr;
};


class PCGEXTENDEDTOOLKIT_API FPCGExBuildCustomGraphElement : public FPCGExCustomGraphProcessorElement
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
