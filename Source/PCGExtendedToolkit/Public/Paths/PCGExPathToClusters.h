// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"


#include "Graph/PCGExUnionHelpers.h"

#include "Graph/PCGExGraph.h"
#include "Graph/PCGExIntersections.h"
#include "PCGExPathToClusters.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPathToClustersSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathsToEdgeClusters, "Path : To Clusters", "Merge paths to edge clusters for glorious pathfinding inception");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterGen; }
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual FName GetMainOutputPin() const override { return PCGExGraph::OutputVerticesLabel; }
	//~End UPCGExPointsProcessorSettings

	/** Whether to fuse paths into a single graph or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bFusePaths = true;

	/** Fuse Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Point/Point Settings", EditCondition="bFusePaths"))
	FPCGExPointPointIntersectionDetails PointPointIntersectionDetails;

	/** Find Point-Edge intersection (points on edges)*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bFusePaths"))
	bool bFindPointEdgeIntersections;

	/** Point-Edge intersection settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Point/Edge Settings", EditCondition="bFusePaths && bFindPointEdgeIntersections"))
	FPCGExPointEdgeIntersectionDetails PointEdgeIntersectionDetails;

	/** Find Edge-Edge intersection (edge crossings)*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bFusePaths"))
	bool bFindEdgeEdgeIntersections;

	/** Edge-Edge intersection settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Edge/Edge Settings", EditCondition="bFusePaths && bFindEdgeEdgeIntersections"))
	FPCGExEdgeEdgeIntersectionDetails EdgeEdgeIntersectionDetails;

	/** Defines how fused point properties and attributes are merged together for fused points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(EditCondition="bFusePaths"))
	FPCGExBlendingDetails DefaultPointsBlendingDetails;

	/** Defines how fused point properties and attributes are merged together for fused edges. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(EditCondition="bFusePaths"))
	FPCGExBlendingDetails DefaultEdgesBlendingDetails;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(PCG_Overridable, EditCondition="bFusePaths"))
	bool bUseCustomPointEdgeBlending = false;

	/** Defines how fused point properties and attributes are merged together for Point/Edge intersections. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(PCG_Overridable, EditCondition="bFusePaths && bUseCustomPointEdgeBlending"))
	FPCGExBlendingDetails CustomPointEdgeBlendingDetails;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(PCG_Overridable, EditCondition="bFusePaths"))
	bool bUseCustomEdgeEdgeBlending = false;

	/** Defines how fused point properties and attributes are merged together for Edge/Edge intersections (Crossings). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(PCG_Overridable, EditCondition="bFusePaths && bUseCustomEdgeEdgeBlending"))
	FPCGExBlendingDetails CustomEdgeEdgeBlendingDetails;

	/** Meta filter settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Carry Over Settings"))
	FPCGExCarryOverDetails CarryOverDetails;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathToClustersContext final : FPCGExPathProcessorContext
{
	friend class FPCGExPathToClustersElement;

	TArray<TSharedRef<PCGExData::FFacade>> PathsFacades;

	FPCGExCarryOverDetails CarryOverDetails;

	TSharedPtr<PCGExGraph::FUnionGraph> UnionGraph;
	TSharedPtr<PCGExData::FFacade> UnionDataFacade;

	TSharedPtr<PCGExGraph::FUnionProcessor> UnionProcessor;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathToClustersElement final : public FPCGExPathProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExPathToClusters
{
#pragma region NonFusing

	class FNonFusingProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExPathToClustersContext, UPCGExPathToClustersSettings>
	{
		bool bClosedLoop = false;

	public:
		TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder;

		explicit FNonFusingProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FNonFusingProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void CompleteWork() override;
	};

#pragma endregion

#pragma region Fusing
	// Fusing processors

	class FFusingProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExPathToClustersContext, UPCGExPathToClustersSettings>
	{
		bool bClosedLoop = false;

		int32 IOIndex = 0;
		int32 LastIndex = 0;

		const TArray<FPCGPoint>* InPoints = nullptr;

	public:
		TSharedPtr<PCGExGraph::FUnionGraph> UnionGraph;

		explicit FFusingProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FFusingProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount) override;
	};

#pragma endregion
}
