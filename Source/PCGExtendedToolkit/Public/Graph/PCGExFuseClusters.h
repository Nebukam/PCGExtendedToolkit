// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExEdgesProcessor.h"

#include "PCGExFuseClusters.generated.h"

/**
 * A Base node to process a set of point using GraphParams.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExFuseClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FuseClusters, "Graph : Fuse Clusters", "Finds Point/Edge and Edge/Edge intersections between all input clusters.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorGraph; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExEdgesProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExEdgesProcessorSettings interface

	/** Fuse Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExFuseSettingsWithTarget FuseSettings;

	/** Point-Edge intersection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bDoPointEdgeIntersection;

	/** Point-Edge intersection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bDoPointEdgeIntersection"))
	FPCGExPointEdgeIntersectionSettings PointEdgeIntersection;

	/** Edge-Edge intersection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bDoEdgeEdgeIntersection;

	/** Edge-Edge intersection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bDoEdgeEdgeIntersection"))
	FPCGExEdgeEdgeIntersectionSettings EdgeEdgeIntersection;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Graph Output Settings"))
	FPCGExGraphBuilderSettings GraphBuilderSettings;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFuseClustersContext : public FPCGExEdgesProcessorContext
{
	friend class UPCGExFuseClustersSettings;
	friend class FPCGExFuseClustersElement;

	virtual ~FPCGExFuseClustersContext() override;

	FPCGExFuseSettingsWithTarget FuseSettings;
	FPCGExPointEdgeIntersectionSettings PointEdgeSettings;
	FPCGExEdgeEdgeIntersectionSettings EdgeEdgeSettings;

	PCGExGraph::FLooseGraph* LooseGraph = nullptr;
	PCGExData::FPointIO* ConsolidatedPoints = nullptr;

	FPCGExGraphBuilderSettings GraphBuilderSettings;
	PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;

	PCGExGraph::FPointEdgeIntersections* PointEdgeIntersections = nullptr;
	PCGExGraph::FEdgeEdgeIntersections* EdgeEdgeIntersections = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExFuseClustersElement : public FPCGExEdgesProcessorElement
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

class PCGEXTENDEDTOOLKIT_API FPCGExFuseClustersInsertLoosePointsTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExFuseClustersInsertLoosePointsTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
	                                        PCGExGraph::FLooseGraph* InGraph, PCGExData::FPointIO* InEdgeIO, TMap<int32, int32>* InNodeIndicesMap)
		: FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		  Graph(InGraph), EdgeIO(InEdgeIO), NodeIndicesMap(InNodeIndicesMap)
	{
	}

	PCGExGraph::FLooseGraph* Graph = nullptr;
	PCGExData::FPointIO* EdgeIO = nullptr;
	TMap<int32, int32>* NodeIndicesMap = nullptr;

	virtual bool ExecuteTask() override;
};
