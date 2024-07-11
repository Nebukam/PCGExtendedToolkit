// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"


#include "Geometry/PCGExGeo.h"

#include "PCGExBuildDelaunayGraph2D.generated.h"

namespace PCGExGeo
{
	class TDelaunay2;
}

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Urquhart Site Merge Mode"))
enum class EPCGExUrquhartSiteMergeMode : uint8
{
	None UMETA(DisplayName = "None", ToolTip="Do not merge sites."),
	MergeSites UMETA(DisplayName = "Merge Sites", ToolTip="Merge site is the average of the merge."),
	MergeEdges UMETA(DisplayName = "Merge Edges", ToolTip="Merge site is the averge of the removed edges."),
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExBuildDelaunayGraph2DSettings : public UPCGExPointsProcessorSettings
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
	FPCGExGraphBuilderDetails GraphBuilderDetails = FPCGExGraphBuilderDetails(false);

private:
	friend class FPCGExBuildDelaunayGraph2DElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBuildDelaunayGraph2DContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExBuildDelaunayGraph2DElement;

	virtual ~FPCGExBuildDelaunayGraph2DContext() override;

	TMap<PCGExData::FPointIO*, PCGExData::FPointIO*> SitesIOMap;
	PCGExData::FPointIOCollection* MainSites = nullptr;
};


class PCGEXTENDEDTOOLKIT_API FPCGExBuildDelaunayGraph2DElement final : public FPCGExPointsProcessorElement
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

namespace PCGExBuildDelaunay2D
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		friend class FOutputDelaunaySites2D;
		friend class FOutputDelaunayUrquhartSites2D;

	protected:
		PCGExGeo::TDelaunay2* Delaunay = nullptr;
		TSet<uint64> UrquhartEdges;
		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
		FPCGExGeo2DProjectionDetails ProjectionDetails;

		PCGEx::TFAttributeWriter<bool>* HullMarkPointWriter = nullptr;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};

	class PCGEXTENDEDTOOLKIT_API FOutputDelaunaySites2D final : public PCGExMT::FPCGExTask
	{
	public:
		FOutputDelaunaySites2D(PCGExData::FPointIO* InPointIO,
		                       FProcessor* InProcessor) :
			FPCGExTask(InPointIO),
			Processor(InProcessor)
		{
		}

		FProcessor* Processor = nullptr;

		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FOutputDelaunayUrquhartSites2D final : public PCGExMT::FPCGExTask
	{
	public:
		FOutputDelaunayUrquhartSites2D(PCGExData::FPointIO* InPointIO,
		                               FProcessor* InProcessor) :
			FPCGExTask(InPointIO),
			Processor(InProcessor)
		{
		}

		FProcessor* Processor = nullptr;

		virtual bool ExecuteTask() override;
	};
}
