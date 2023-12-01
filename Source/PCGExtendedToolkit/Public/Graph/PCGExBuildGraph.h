// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExGraphProcessor.h"
#include "Solvers/PCGExGraphSolver.h"

#include "PCGExBuildGraph.generated.h"

constexpr PCGExMT::AsyncState State_ProbingPoints = 200;

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
	PCGEX_NODE_INFOS(BuildGraph, "Build Graph", "Write graph data to an attribute for each connected Graph Params. `Build Graph` uses the socket information as is.");
#endif
protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface
	
public:
	/** Compute edge types internally. If you don't need edge types, set it to false to save some cycles.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bComputeEdgeType = true;

	/** Ignores candidates weighting pass and always favors the closest one.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, Instanced)
	UPCGExGraphSolver* GraphSolver;
	
	virtual FName GetMainPointsInputLabel() const override;
	virtual int32 GetPreferredChunkSize() const override;

	virtual PCGExIO::EInitMode GetPointOutputInitMode() const override;

private:
	friend class FPCGExBuildGraphElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBuildGraphContext : public FPCGExGraphProcessorContext
{
	friend class FPCGExBuildGraphElement;

public:
	UPCGExGraphSolver* GraphSolver;
	UPCGPointData::PointOctree* Octree = nullptr;
	bool bComputeEdgeType = true;
	bool bMoveSocketOriginOnPointExtent = false;
};


class PCGEXTENDEDTOOLKIT_API FPCGExBuildGraphElement : public FPCGExGraphProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
