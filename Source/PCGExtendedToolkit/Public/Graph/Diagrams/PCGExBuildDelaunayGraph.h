// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"

#include "Geometry/PCGExGeoDelaunay.h"

#include "PCGExBuildDelaunayGraph.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBuildDelaunayGraphSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildDelaunayGraph, "Cluster : Delaunay 3D", "Create a 3D delaunay tetrahedralization for each input dataset.");
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
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sites", meta = (PCG_Overridable, EditCondition="bUrquhart && bOutputSites", EditConditionHides))
	bool bMergeUrquhartSites = false;

	/** Mark points & edges that lie on the hull */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bMarkHull = true;

	/** Name of the attribute to output the Hull boolean to. True if point is on the hull, otherwise false. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bMarkHull"))
	FName HullAttributeName = "bIsOnHull";

	/** When true, edges that have at least a point on the Hull as marked as being on the hull. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bMarkEdgeOnTouch = false;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails = FPCGExGraphBuilderDetails();

private:
	friend class FPCGExBuildDelaunayGraphElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBuildDelaunayGraphContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExBuildDelaunayGraphElement;

	TSharedPtr<PCGExData::FPointIOCollection> MainSites;
};


class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBuildDelaunayGraphElement final : public FPCGExPointsProcessorElement
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

namespace PCGExBuildDelaunay
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExBuildDelaunayGraphContext, UPCGExBuildDelaunayGraphSettings>
	{
		friend class FOutputDelaunaySites;
		friend class FOutputDelaunayUrquhartSites;

	protected:
		TUniquePtr<PCGExGeo::TDelaunay3> Delaunay;
		TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder;
		TSet<uint64> UrquhartEdges;

		TSharedPtr<PCGExData::TBuffer<bool>> HullMarkPointWriter;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FOutputDelaunaySites final : public PCGExMT::FPCGExTask
	{
	public:
		FOutputDelaunaySites(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		                     const TSharedPtr<FProcessor>& InProcessor) :
			FPCGExTask(InPointIO),
			Processor(InProcessor)
		{
		}

		TSharedPtr<FProcessor> Processor;

		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FOutputDelaunayUrquhartSites final : public PCGExMT::FPCGExTask
	{
	public:
		FOutputDelaunayUrquhartSites(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		                             const TSharedPtr<FProcessor>& InProcessor) :
			FPCGExTask(InPointIO),
			Processor(InProcessor)
		{
		}

		TSharedPtr<FProcessor> Processor;

		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};
}
