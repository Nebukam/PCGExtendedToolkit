// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Graph/PCGExIntersections.h"
#include "PCGExPathToClusters.generated.h"

namespace PCGExGraph
{
	struct FCompoundProcessor;
}

namespace PCGExDataBlending
{
	class FMetadataBlender;
	class FCompoundBlender;
}

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExPathToClustersSettings : public UPCGExPathProcessorSettings
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
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual FName GetMainOutputLabel() const override { return PCGExGraph::OutputVerticesLabel; }
	//~End UPCGExPointsProcessorSettings

public:
	/** Consider paths to be closed -- processing will wrap between first and last points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bClosedPath = false;

	/** Whether to fuse paths into a single graph or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, NoEditInline))
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

struct PCGEXTENDEDTOOLKIT_API FPCGExPathToClustersContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExPathToClustersElement;

	virtual ~FPCGExPathToClustersContext() override;

	TArray<PCGExData::FFacade*> PathsFacades;

	FPCGExCarryOverDetails CarryOverDetails;

	PCGExGraph::FCompoundGraph* CompoundGraph = nullptr;
	PCGExData::FFacade* CompoundFacade = nullptr;

	PCGExGraph::FCompoundProcessor* CompoundProcessor = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPathToClustersElement final : public FPCGExPathProcessorElement
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

namespace PCGExPathToClusters
{
#pragma region NonFusing

	class FNonFusingProcessor final : public PCGExPointsMT::FPointsProcessor
	{
	public:
		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;

		explicit FNonFusingProcessor(PCGExData::FPointIO* InPoints)
			: FPointsProcessor(InPoints)
		{
		}

		virtual ~FNonFusingProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void CompleteWork() override;
	};

#pragma endregion

#pragma region Fusing
	// Fusing processors

	class FFusingProcessor final : public PCGExPointsMT::FPointsProcessor
	{
	public:
		PCGExGraph::FCompoundGraph* CompoundGraph = nullptr;

		explicit FFusingProcessor(PCGExData::FPointIO* InPoints)
			: FPointsProcessor(InPoints)
		{
		}

		virtual ~FFusingProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
	};

#pragma endregion
}
