// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExPathsToEdgeClusters.generated.h"

namespace PCGExDataBlending
{
	class FCompoundBlender;
}

namespace PCGExGraph
{
	class FGraph;
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExPathsToEdgeClustersSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPathsToEdgeClustersSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathsToEdgeClusters, "Path : To Edge Clusters", "Merge paths to edge clusters for glorious pathfinding inception");
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

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
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual FName GetMainInputLabel() const override;
	virtual FName GetMainOutputLabel() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Consider paths to be closed -- processing will wrap between first and last points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bClosedPath = false;

	/** Consider paths to be closed -- processing will wrap between first and last points. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bFusePaths = true;

	/** Fuse Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Point/Point", EditCondition="bFusePaths"))
	FPCGExPointPointIntersectionSettings PointPointIntersectionSettings;

	/** Point-Edge intersection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bDoPointEdgeIntersection;

	/** Point-Edge intersection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Point/Edge", EditCondition="bFusePaths && bDoPointEdgeIntersection"))
	FPCGExPointEdgeIntersectionSettings PointEdgeIntersectionSettings;

	/** Edge-Edge intersection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bDoEdgeEdgeIntersection;

	/** Edge-Edge intersection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Edge/Edge", EditCondition="bFusePaths && bDoEdgeEdgeIntersection"))
	FPCGExEdgeEdgeIntersectionSettings EdgeEdgeIntersectionSettings;

	/** Defines how fused point properties and attributes are merged together for fused points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExBlendingSettings PointsBlendingSettings;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Graph Output Settings"))
	FPCGExGraphBuilderSettings GraphBuilderSettings;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPathsToEdgeClustersContext : public FPCGExPathProcessorContext
{
	friend class FPCGExPathsToEdgeClustersElement;

	virtual ~FPCGExPathsToEdgeClustersContext() override;

	FPCGExPointPointIntersectionSettings PointPointIntersectionSettings;
	FPCGExPointEdgeIntersectionSettings PointEdgeIntersectionSettings;
	FPCGExEdgeEdgeIntersectionSettings EdgeEdgeIntersectionSettings;

	PCGExGraph::FCompoundGraph* CompoundGraph;
	PCGExDataBlending::FCompoundBlender* CompoundPointsBlender = nullptr;

	PCGExData::FPointIO* ConsolidatedPoints = nullptr;

	FPCGExGraphBuilderSettings GraphBuilderSettings;
	PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;

	PCGExGraph::FPointEdgeIntersections* PointEdgeIntersections = nullptr;
	PCGExGraph::FEdgeEdgeIntersections* EdgeEdgeIntersections = nullptr;

	PCGExGraph::FGraphMetadataSettings GraphMetadataSettings;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPathsToEdgeClustersElement : public FPCGExPathProcessorElement
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


class PCGEXTENDEDTOOLKIT_API FPCGExInsertPathToCompoundGraphTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExInsertPathToCompoundGraphTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
	                                    PCGExGraph::FCompoundGraph* InGraph,
	                                    const bool bInJoinFirstAndLast)
		: FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		  Graph(InGraph),
		  bJoinFirstAndLast(bInJoinFirstAndLast)
	{
	}

	PCGExGraph::FCompoundGraph* Graph = nullptr;
	bool bJoinFirstAndLast = false;

	virtual bool ExecuteTask() override;
};
