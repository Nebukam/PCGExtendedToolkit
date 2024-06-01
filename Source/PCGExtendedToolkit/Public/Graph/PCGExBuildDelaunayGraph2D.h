// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExCustomGraphProcessor.h"
#include "Geometry/PCGExGeo.h"

#include "PCGExBuildDelaunayGraph2D.generated.h"

namespace PCGExGeo
{
	class TConvexHull2;
	class TDelaunayTriangulation2;
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExBuildDelaunayGraph2DSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildDelaunayGraph2D, "Graph : Delaunay 2D", "Create a 2D delaunay triangulation for each input dataset.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorGraphGen; }
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual FName GetMainOutputLabel() const override;
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Output the Urquhart graph of the Delaunay triangulation (removes the longest edge of each Delaunay cell) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bUrquhart = false;

	/** Mark points & edges that lie on the hull */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bMarkHull = true;

	/** Name of the attribute to output the Hull boolean to. True if point is on the hull, otherwise false. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bMarkHull"))
	FName HullAttributeName = "bIsOnHull";

	/** When true, edges that have at least a point on the Hull as marked as being on the hull. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bMarkEdgeOnTouch = false;

	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionSettings ProjectionSettings;

private:
	friend class FPCGExBuildDelaunayGraph2DElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBuildDelaunayGraph2DContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExBuildDelaunayGraph2DElement;

	virtual ~FPCGExBuildDelaunayGraph2DContext() override;

	TSet<int32> HullIndices;

	FPCGExGeo2DProjectionSettings ProjectionSettings;

	FPCGExGraphBuilderSettings GraphBuilderSettings;
	PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
};


class PCGEXTENDEDTOOLKIT_API FPCGExBuildDelaunayGraph2DElement : public FPCGExPointsProcessorElementBase
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

class PCGEXTENDEDTOOLKIT_API FPCGExDelaunay2Task : public FPCGExNonAbandonableTask
{
public:
	FPCGExDelaunay2Task(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		PCGExGraph::FGraph* InGraph) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		Graph(InGraph)
	{
	}

	PCGExGraph::FGraph* Graph = nullptr;

	virtual bool ExecuteTask() override;
};
