// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Graph/PCGExIntersections.h"
#include "PCGExPathToEdgeClusters.generated.h"

namespace PCGExDataBlending
{
	class FMetadataBlender;
	class FCompoundBlender;
}

namespace PCGExGraph
{
	struct FEdgeEdgeIntersections;
	struct FPointEdgeIntersections;
	struct FCompoundGraph;
	class FGraph;
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExPathToEdgeClustersSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPathToEdgeClustersSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathsToEdgeClusters, "Path : To Clusters", "Merge paths to edge clusters for glorious pathfinding inception");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorGraphGen; }
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

	/** Whether to fuse paths into a single graph or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, NoEditInline))
	bool bFusePaths = true;

	/** Fuse Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Point/Point Settings", EditCondition="bFusePaths"))
	FPCGExPointPointIntersectionSettings PointPointIntersectionSettings;

	/** Find Point-Edge intersection (points on edges)*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bFusePaths"))
	bool bFindPointEdgeIntersections;

	/** Point-Edge intersection settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Point/Edge Settings", EditCondition="bFusePaths && bFindPointEdgeIntersections"))
	FPCGExPointEdgeIntersectionSettings PointEdgeIntersectionSettings;

	/** Find Edge-Edge intersection (edge crossings)*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bFusePaths"))
	bool bFindEdgeEdgeIntersections;

	/** Edge-Edge intersection settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Edge/Edge Settings", EditCondition="bFusePaths && bFindEdgeEdgeIntersections"))
	FPCGExEdgeEdgeIntersectionSettings EdgeEdgeIntersectionSettings;

	/** Defines how fused point properties and attributes are merged together for fused points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(EditCondition="bFusePaths"))
	FPCGExBlendingSettings DefaultPointsBlendingSettings;

	/** Defines how fused point properties and attributes are merged together for fused edges. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(EditCondition="bFusePaths"))
	FPCGExBlendingSettings DefaultEdgesBlendingSettings;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(PCG_Overridable, EditCondition="bFusePaths"))
	bool bUseCustomPointEdgeBlending = false;

	/** Defines how fused point properties and attributes are merged together for Point/Edge intersections. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(PCG_Overridable, EditCondition="bFusePaths && bUseCustomPointEdgeBlending"))
	FPCGExBlendingSettings CustomPointEdgeBlendingSettings;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(PCG_Overridable, EditCondition="bFusePaths"))
	bool bUseCustomEdgeEdgeBlending = false;

	/** Defines how fused point properties and attributes are merged together for Edge/Edge intersections (Crossings). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(PCG_Overridable, EditCondition="bFusePaths && bUseCustomEdgeEdgeBlending"))
	FPCGExBlendingSettings CustomEdgeEdgeBlendingSettings;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Graph Output Settings"))
	FPCGExGraphBuilderSettings GraphBuilderSettings;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPathToEdgeClustersContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExPathToEdgeClustersElement;

	virtual ~FPCGExPathToEdgeClustersContext() override;

	PCGExGraph::FCompoundGraph* CompoundGraph;
	PCGExDataBlending::FCompoundBlender* CompoundPointsBlender = nullptr;

	PCGExData::FPointIO* ConsolidatedPoints = nullptr;

	FPCGExGraphBuilderSettings GraphBuilderSettings;
	PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;

	PCGExGraph::FGraphMetadataSettings GraphMetadataSettings;
	PCGExGraph::FPointEdgeIntersections* PointEdgeIntersections = nullptr;
	PCGExGraph::FEdgeEdgeIntersections* EdgeEdgeIntersections = nullptr;
	PCGExDataBlending::FMetadataBlender* MetadataBlender = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPathToEdgeClustersElement final : public FPCGExPathProcessorElement
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


class PCGEXTENDEDTOOLKIT_API FPCGExInsertPathToCompoundGraphTask final : public FPCGExNonAbandonableTask
{
public:
	FPCGExInsertPathToCompoundGraphTask(PCGExData::FPointIO* InPointIO,
	                                    PCGExGraph::FCompoundGraph* InGraph,
	                                    const bool bInJoinFirstAndLast)
		: FPCGExNonAbandonableTask(InPointIO),
		  Graph(InGraph),
		  bJoinFirstAndLast(bInJoinFirstAndLast)
	{
	}

	PCGExGraph::FCompoundGraph* Graph = nullptr;
	bool bJoinFirstAndLast = false;

	virtual bool ExecuteTask() override;
};


namespace PCGExPathToClusters
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
	public:
		PCGExGraph::FCompoundGraph* CompoundGraph = nullptr;

		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
		FPCGExGraphBuilderSettings GraphBuilderSettings;

		explicit FProcessor(PCGExData::FPointIO* InPoints);
		virtual ~FProcessor() override;

		virtual bool Process(FPCGExAsyncManager* AsyncManager) override;
		virtual void ProcessSinglePoint(int32 Index, FPCGPoint& Point) override;
		virtual void CompleteWork() override;
	};

	class FProcessorBatch final : public PCGExPointsMT::TBatch<FProcessor>
	{
	public:
		PCGExGraph::FCompoundGraph* CompoundGraph = nullptr;
		PCGExData::FPointIO* CompoundPoints = nullptr;

		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
		FPCGExGraphBuilderSettings GraphBuilderSettings;

		FProcessorBatch(FPCGContext* InContext, const TArray<PCGExData::FPointIO*>& InPointsCollection);

		virtual bool PrepareSingle(FProcessor* ClusterProcessor) override;
	};
}
