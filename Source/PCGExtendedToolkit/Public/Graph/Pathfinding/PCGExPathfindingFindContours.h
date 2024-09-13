// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExDataForward.h"
#include "Geometry/PCGExGeo.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExPathfindingFindContours.generated.h"

namespace PCGExFindContours
{
	class FProcessor;
	const FName OutputGoodSeedsLabel = TEXT("SeedGenSuccess");
	const FName OutputBadSeedsLabel = TEXT("SeedGenFailed");
}

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Contour Shape Type Output"))
enum class EPCGExContourShapeTypeOutput : uint8
{
	Both        = 0 UMETA(DisplayName = "Convex & Concave", ToolTip="Output both convex and concave paths"),
	ConvexOnly  = 1 UMETA(DisplayName = "Convex Only", ToolTip="Output only convex paths"),
	ConcaveOnly = 2 UMETA(DisplayName = "Concave Only", ToolTip="Output only concave paths")
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFindContoursSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindContours, "Pathfinding : Find Contours", "Attempts to find a closed contour of connected edges around seed points.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorPathfinding; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

	/** Drive how a seed selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExNodeSelectionDetails SeedPicking;

	/** Whether or not to duplicate dead end points. Useful if you plan on offsetting the generated contours. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bDuplicateDeadEndPoints = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta = (PCG_Overridable))
	EPCGExContourShapeTypeOutput OutputType = EPCGExContourShapeTypeOutput::Both;

	/** Ensure the node doesn't output duplicate path. Can be expensive. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta = (PCG_Overridable))
	bool bDedupePaths = true;

	/** Keep only contours that closed gracefully; i.e connect to their start node */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta = (PCG_Overridable))
	bool bKeepOnlyGracefulContours = true;

	/** Whether to keep contour that include dead ends wrapping */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta = (PCG_Overridable))
	bool bKeepContoursWithDeadEnds = true;

	/** Output a filtered out set of points containing only those that generated a path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta = (PCG_Overridable))
	bool bOutputFilteredSeeds = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitBelowPointCount = false;

	/** Paths with a point count below the specified threshold will be omitted */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta = (PCG_Overridable, EditCondition="bOmitBelowPointCount", ClampMin=0))
	int32 MinPointCount = 3;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitAbovePointCount = false;

	/** Paths with a point count below the specified threshold will be omitted */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta = (PCG_Overridable, EditCondition="bOmitAbovePointCount", ClampMin=0))
	int32 MaxPointCount = 500;

	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExAttributeToTagDetails SeedAttributesToPathTags;

	/** Which Seed attributes to forward on paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExForwardDetails SeedForwarding;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(InlineEditConditionToggle))
	bool bTagConcave = false;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="bTagConcave"))
	FString ConcaveTag = TEXT("Concave");

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(InlineEditConditionToggle))
	bool bTagConvex = false;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="bTagConvex"))
	FString ConvexTag = TEXT("Convex");

	/** Whether to flag path points generated from "dead ends" */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(InlineEditConditionToggle))
	bool bFlagDeadEnds = false;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="bFlagDeadEnds"))
	FName DeadEndAttributeName = TEXT("IsDeadEnd");

	/** Whether or not to search for closest node using an octree. Depending on your dataset, enabling this may be either much faster, or much slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance", meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bUseOctreeSearch = false;

private:
	friend class FPCGExFindContoursElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFindContoursContext final : public FPCGExEdgesProcessorContext
{
	friend class FPCGExFindContoursElement;
	friend class FPCGExCreateBridgeTask;

	virtual ~FPCGExFindContoursContext() override;

	FPCGExGeo2DProjectionDetails ProjectionDetails;
	PCGExData::FFacade* SeedsDataFacade = nullptr;

	PCGExData::FPointIOCollection* Paths;
	PCGExData::FPointIO* GoodSeeds;
	PCGExData::FPointIO* BadSeeds;

	TArray<bool> SeedQuality;

	FPCGExAttributeToTagDetails SeedAttributesToPathTags;
	PCGExData::FDataForwardHandler* SeedForwardHandler;

	bool TryFindContours(PCGExData::FPointIO* PathIO, const int32 SeedIndex, PCGExFindContours::FProcessor* ClusterProcessor);
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFindContoursElement final : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExFindContours
{
	class FProcessor final : public PCGExClusterMT::FClusterProcessor
	{
		friend struct FPCGExFindContoursContext;
		friend class FBatch;

		mutable FRWLock UniquePathsLock;
		TSet<uint32> UniquePathsBounds;
		TSet<uint64> UniquePathsStartPairs;

	protected:
		const UPCGExFindContoursSettings* LocalSettings = nullptr;
		FPCGExFindContoursContext* LocalTypedContext = nullptr;

		TArray<FVector>* ProjectedPositions = nullptr;

		bool bBuildExpandedNodes = false;
		TArray<PCGExCluster::FExpandedNode*>* ExpandedNodes = nullptr;
		TArray<PCGExCluster::FExpandedEdge*>* ExpandedEdges = nullptr;

	public:
		FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
			FClusterProcessor(InVtx, InEdges)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSingleRangeIteration(int32 Iteration, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;

		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		friend class FProjectRangeTask;

	protected:
		FPCGExGeo2DProjectionDetails ProjectionDetails;
		TArray<FVector> ProjectedPositions;

	public:
		FBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, const TArrayView<PCGExData::FPointIO*> InEdges):
			TBatch(InContext, InVtx, InEdges)
		{
		}

		virtual void Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual bool PrepareSingle(FProcessor* ClusterProcessor) override;
	};

	class FProjectRangeTask : public PCGExMT::FPCGExTask
	{
	public:
		FProjectRangeTask(PCGExData::FPointIO* InPointIO,
		                  FBatch* InBatch):
			FPCGExTask(InPointIO),
			Batch(InBatch)
		{
		}

		FBatch* Batch = nullptr;
		int32 NumIterations = 0;
		virtual bool ExecuteTask() override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFindContourTask final : public PCGExMT::FPCGExTask
	{
	public:
		FPCGExFindContourTask(PCGExData::FPointIO* InPointIO,
		                      FProcessor* InClusterProcessor) :
			FPCGExTask(InPointIO),
			ClusterProcessor(InClusterProcessor)
		{
		}

		FProcessor* ClusterProcessor = nullptr;

		virtual bool ExecuteTask() override;
	};
}
