// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"


#include "Geometry/PCGExGeo.h"
#include "Geometry/PCGExGeoDelaunay.h"

#include "PCGExBuildDelaunayGraph2D.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Urquhart Site Merge Mode"))
enum class EPCGExUrquhartSiteMergeMode : uint8
{
	None       = 0 UMETA(DisplayName = "None", ToolTip="Do not merge sites."),
	MergeSites = 1 UMETA(DisplayName = "Merge Sites", ToolTip="Merge site is the average of the merge."),
	MergeEdges = 2 UMETA(DisplayName = "Merge Edges", ToolTip="Merge site is the averge of the removed edges."),
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBuildDelaunayGraph2DSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildDelaunayGraph2D, "Cluster : Delaunay 2D", "Create a 2D delaunay triangulation for each input dataset.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterGen; }
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual FName GetMainOutputLabel() const override { return PCGExGraph::OutputVerticesLabel; }
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** Output the Urquhart graph of the Delaunay triangulation (removes the longest edge of each Delaunay cell) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bUrquhart = false;

	/** Output delaunay sites */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sites", meta = (PCG_Overridable))
	bool bOutputSites = false;

	/** Mark points & edges that lie on the hull */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sites", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bMarkSiteHull = true;

	/** Name of the attribute to output the Hull boolean to. True if point is on the hull, otherwise false. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sites", meta = (PCG_Overridable, EditCondition="bMarkSiteHull"))
	FName SiteHullAttributeName = "bIsOnHull";

	/** Merge adjacent sites into a single point */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sites", meta = (PCG_Overridable, EditCondition="bUrquhart && bOutputSites", EditConditionHides))
	EPCGExUrquhartSiteMergeMode UrquhartSitesMerge = EPCGExUrquhartSiteMergeMode::None;

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
	FPCGExGeo2DProjectionDetails ProjectionDetails;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails = FPCGExGraphBuilderDetails();

private:
	friend class FPCGExBuildDelaunayGraph2DElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBuildDelaunayGraph2DContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExBuildDelaunayGraph2DElement;

	TSharedPtr<PCGExData::FPointIOCollection> MainSites;
};


class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBuildDelaunayGraph2DElement final : public FPCGExPointsProcessorElement
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

namespace PCGExBuildDelaunay2D
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExBuildDelaunayGraph2DContext, UPCGExBuildDelaunayGraph2DSettings>
	{
		friend class FOutputDelaunaySites2D;
		friend class FOutputDelaunayUrquhartSites2D;

	protected:
		TUniquePtr<PCGExGeo::TDelaunay2> Delaunay;
		TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder;
		TSet<uint64> UrquhartEdges;
		FPCGExGeo2DProjectionDetails ProjectionDetails;

		TSharedPtr<PCGExData::TBuffer<bool>> HullMarkPointWriter;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FOutputDelaunaySites2D final : public PCGExMT::FPCGExTask
	{
	public:
		FOutputDelaunaySites2D(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		                       FProcessor* InProcessor) :
			FPCGExTask(InPointIO),
			Processor(InProcessor)
		{
		}

		FProcessor* Processor = nullptr;

		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FOutputDelaunayUrquhartSites2D final : public PCGExMT::FPCGExTask
	{
	public:
		FOutputDelaunayUrquhartSites2D(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		                               FProcessor* InProcessor) :
			FPCGExTask(InPointIO),
			Processor(InProcessor)
		{
		}

		FProcessor* Processor = nullptr;

		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};
}
