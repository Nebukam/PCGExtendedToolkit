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

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Contour Shape Type Output")--E*/)
enum class EPCGExContourShapeTypeOutput : uint8
{
	Both        = 0 UMETA(DisplayName = "Convex & Concave", ToolTip="Output both convex and concave paths"),
	ConvexOnly  = 1 UMETA(DisplayName = "Convex Only", ToolTip="Output only convex paths"),
	ConcaveOnly = 2 UMETA(DisplayName = "Concave Only", ToolTip="Output only concave paths")
};

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Contour Shape Type Output")--E*/)
enum class EPCGExGoodSeedPlacement : uint8
{
	Original         = 0 UMETA(DisplayName = "Original", ToolTip="Seed position is unchanged"),
	Centroid         = 1 UMETA(DisplayName = "Centroid", ToolTip="Place the seed at the centroid of the path"),
	FirstPoint       = 2 UMETA(DisplayName = "First point", ToolTip="Place the seed at the first point of the path"),
	PathBoundsCenter = 3 UMETA(DisplayName = "Path bounds center", ToolTip="Place the seed at the center of the path' bounds")
};

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Contour Shape Type Output")--E*/)
enum class EPCGExGoodSeedBounds : uint8
{
	Original           = 0 UMETA(DisplayName = "Original", ToolTip="Seed bounds is unchanged"),
	MatchPath          = 1 UMETA(DisplayName = "Match Path", ToolTip="Seed bounds match path bounds"),
	MatchPathResetQuat = 2 UMETA(DisplayName = "Match Path (with rotation reset)", ToolTip="Seed bounds match path bounds, and rotation is reset"),
};

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Contour Shape Type Output")--E*/)
enum class EPCGExOmitPathsByBounds : uint8
{
	None                     = 0 UMETA(DisplayName = "None", ToolTip="Seed position is unchanged"),
	NearlyEqualClusterBounds = 1 UMETA(DisplayName = "Nearly Equal to Cluster", ToolTip="Remove path whose bounds are nearly equal to cluster's"),
	SizeCheck                = 2 UMETA(DisplayName = "Size Check", ToolTip="Puts limits based on bounds' size length"),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
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
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	/** Drive how a seed selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExNodeSelectionDetails SeedPicking;

	/** Whether or not to duplicate dead end points. Useful if you plan on offsetting the generated contours. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bDuplicateDeadEndPoints = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable))
	EPCGExContourShapeTypeOutput OutputType = EPCGExContourShapeTypeOutput::Both;

	/** Ensure the node doesn't output duplicate path. Can be expensive. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable))
	bool bDedupePaths = true;

	/** Keep only contours that closed gracefully; i.e connect to their start node */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable))
	bool bKeepOnlyGracefulContours = true;

	/** Whether to keep contour that include dead ends wrapping */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable))
	bool bKeepContoursWithDeadEnds = true;

	/** Output a filtered out set of points containing only those that generated a path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable))
	bool bOutputFilteredSeeds = false;

	/** Change the good seed position */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, EditCondition="bOutputFilteredSeeds", EditConditionHides))
	EPCGExGoodSeedPlacement SeedPlacement = EPCGExGoodSeedPlacement::Centroid;

	/** Change the good seed bounds */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, EditCondition="bOutputFilteredSeeds", EditConditionHides))
	EPCGExGoodSeedBounds SeedBounds = EPCGExGoodSeedBounds::MatchPath;

	/** Change the good seed bounds */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, EditCondition="bOutputFilteredSeeds", EditConditionHides))
	EPCGExOmitPathsByBounds OmitPathsByBounds = EPCGExOmitPathsByBounds::None;

	/** Size tolerance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, EditCondition="OmitPathsByBounds==EPCGExOmitPathsByBounds::NearlyEqualClusterBounds", EditConditionHides, ClampMin=0))
	double BoundsSizeTolerance = 100;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs|Bounds Limits", meta = (PCG_Overridable, EditCondition="OmitPathsByBounds==EPCGExOmitPathsByBounds::NearlyEqualClusterBounds", EditConditionHides))
	bool bOmitBelowBoundsSize = false;

	/** Paths with a point count below the specified threshold will be omitted */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs|Bounds Limits", meta = (PCG_Overridable, EditCondition="bOmitBelowBoundsSize && OmitPathsByBounds==EPCGExOmitPathsByBounds::NearlyEqualClusterBounds", EditConditionHides, ClampMin=0))
	double MinBoundsSize = 3;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs|Bounds Limits", meta = (PCG_Overridable, EditCondition="OmitPathsByBounds==EPCGExOmitPathsByBounds::NearlyEqualClusterBounds", EditConditionHides))
	bool bOmitAboveBoundsSize = false;

	/** Paths with a point count below the specified threshold will be omitted */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs|Bounds Limits", meta = (PCG_Overridable, EditCondition="bOmitAboveBoundsSize && OmitPathsByBounds==EPCGExOmitPathsByBounds::NearlyEqualClusterBounds", EditConditionHides, ClampMin=0))
	double MaxBoundsSize = 500;
	
	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitBelowPointCount = false;

	/** Paths with a point count below the specified threshold will be omitted */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, EditCondition="bOmitBelowPointCount", ClampMin=0))
	int32 MinPointCount = 3;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitAbovePointCount = false;

	/** Paths with a point count below the specified threshold will be omitted */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, EditCondition="bOmitAbovePointCount", ClampMin=0))
	int32 MaxPointCount = 500;

	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagConcave = false;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagConcave"))
	FString ConcaveTag = TEXT("Concave");

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagConvex = false;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagConvex"))
	FString ConvexTag = TEXT("Convex");

	/** Whether to flag path points generated from "dead ends" */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bFlagDeadEnds = false;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(DisplayName="DeadEnd Flag", EditCondition="bFlagDeadEnds"))
	FName DeadEndAttributeName = TEXT("IsDeadEnd");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bTagIfClosedLoop = true;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, EditCondition="bTagIfClosedLoop"))
	FString IsClosedLoopTag = TEXT("ClosedLoop");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bTagIfOpenPath = false;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, EditCondition="bTagIfOpenPath"))
	FString IsOpenPathTag = TEXT("OpenPath");

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Forwarding")
	FPCGExAttributeToTagDetails SeedAttributesToPathTags;

	/** Which Seed attributes to forward on paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Forwarding")
	FPCGExForwardDetails SeedForwarding;

	/** Whether or not to search for closest node using an octree. Depending on your dataset, enabling this may be either much faster, or much slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance", meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bUseOctreeSearch = false;

private:
	friend class FPCGExFindContoursElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFindContoursContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExFindContoursElement;
	friend class FPCGExCreateBridgeTask;

	FPCGExGeo2DProjectionDetails ProjectionDetails;
	TSharedPtr<PCGExData::FFacade> SeedsDataFacade;

	TSharedPtr<PCGExData::FPointIOCollection> Paths;
	TSharedPtr<PCGExData::FPointIO> GoodSeeds;
	TSharedPtr<PCGExData::FPointIO> BadSeeds;

	TArray<int8> SeedQuality;
	TArray<FPCGPoint> UdpatedSeedPoints;

	FPCGExAttributeToTagDetails SeedAttributesToPathTags;
	TSharedPtr<PCGExData::FDataForwardHandler> SeedForwardHandler;

	bool TryFindContours(const TSharedPtr<PCGExData::FPointIO>& PathIO, const int32 SeedIndex, TSharedPtr<PCGExFindContours::FProcessor> ClusterProcessor);
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
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExFindContoursContext, UPCGExFindContoursSettings>
	{
		friend struct FPCGExFindContoursContext;
		friend class FBatch;

		mutable FRWLock UniquePathsBoxHashLock;
		mutable FRWLock UniquePathsStartHashLock;
		TSet<uint32> UniquePathsBoxHash;
		TSet<uint64> UniquePathsStartHash;

	protected:
		bool bBuildExpandedNodes = false;

	public:
		TArray<FVector>* ProjectedPositions = nullptr;
		TSharedPtr<TArray<PCGExCluster::FExpandedNode>> ExpandedNodes;
		TSharedPtr<TArray<PCGExCluster::FExpandedEdge>> ExpandedEdges;

		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSingleRangeIteration(int32 Iteration, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;

		bool RegisterStartHash(const uint64 Hash);
		bool RegisterBoxHash(const uint64 Hash);
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		friend class FProjectRangeTask;

	protected:
		FPCGExGeo2DProjectionDetails ProjectionDetails;
		TArray<FVector> ProjectedPositions;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges):
			TBatch(InContext, InVtx, InEdges)
		{
		}

		virtual void Process() override;
		virtual bool PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor) override;
	};

	class FProjectRangeTask final : public PCGExMT::FPCGExTask
	{
	public:
		FProjectRangeTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		                  const TSharedPtr<FBatch>& InBatch):
			FPCGExTask(InPointIO),
			Batch(InBatch)
		{
		}

		TSharedPtr<FBatch> Batch;
		int32 NumIterations = 0;
		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFindContourTask final : public PCGExMT::FPCGExTask
	{
	public:
		FPCGExFindContourTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		                      const TSharedPtr<FProcessor>& InClusterProcessor) :
			FPCGExTask(InPointIO),
			ClusterProcessor(InClusterProcessor)
		{
		}

		TSharedPtr<FProcessor> ClusterProcessor;

		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};
}
