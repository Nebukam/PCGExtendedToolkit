﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"


#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExGraph.h"

#include "PCGExBuildDelaunayGraph.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="clusters/diagrams/delaunay-3d"))
class UPCGExBuildDelaunayGraphSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildDelaunayGraph, "Cluster : Delaunay 3D", "Create a 3D delaunay tetrahedralization for each input dataset.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorClusterGenerator; }
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual FName GetMainOutputPin() const override { return PCGExGraph::OutputVerticesLabel; }
	//~End UPCGExPointsProcessorSettings

	/** Output the Urquhart graph of the Delaunay triangulation (removes the longest edge of each Delaunay cell) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bUrquhart = false;

	/** Output delaunay sites */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sites", meta = (PCG_Overridable))
	bool bOutputSites = false;

	/** Mark points & edges that lie on the hull */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sites", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bMarkSiteHull = false;

	/** Name of the attribute to output the Hull boolean to. True if point is on the hull, otherwise false. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sites", meta = (PCG_Overridable, EditCondition="bMarkSiteHull"))
	FName SiteHullAttributeName = "bIsOnHull";

	/** Merge adjacent sites into a single point */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sites", meta = (PCG_Overridable, EditCondition="bUrquhart && bOutputSites", EditConditionHides))
	bool bMergeUrquhartSites = false;

	/** Mark points & edges that lie on the hull */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bMarkHull = false;

	/** Name of the attribute to output the Hull boolean to. True if point is on the hull, otherwise false. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bMarkHull"))
	FName HullAttributeName = "bIsOnHull";

	/** When true, edges that have at least a point on the Hull as marked as being on the hull. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bMarkEdgeOnTouch = false;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails = FPCGExGraphBuilderDetails(EPCGExMinimalAxis::X);

private:
	friend class FPCGExBuildDelaunayGraphElement;
};

struct FPCGExBuildDelaunayGraphContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExBuildDelaunayGraphElement;

	TSharedPtr<PCGExData::FPointIOCollection> MainSites;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExBuildDelaunayGraphElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(BuildDelaunayGraph)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExBuildDelaunayGraph
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExBuildDelaunayGraphContext, UPCGExBuildDelaunayGraphSettings>
	{
		friend class FOutputDelaunaySites;
		friend class FOutputDelaunayUrquhartSites;

	protected:
		TSharedPtr<TArray<int32>> OutputIndices;
		TUniquePtr<PCGExGeo::TDelaunay3> Delaunay;
		TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder;
		TSet<uint64> UrquhartEdges;

		TSharedPtr<PCGExData::TBuffer<bool>> HullMarkPointWriter;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;

		virtual void CompleteWork() override;

		virtual void Write() override;
		virtual void Output() override;
	};

	class FOutputDelaunaySites final : public PCGExMT::FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FOutputDelaunaySites)

		FOutputDelaunaySites(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		                     const TSharedPtr<FProcessor>& InProcessor) :
			FTask(),
			PointIO(InPointIO),
			Processor(InProcessor)
		{
		}

		const TSharedPtr<PCGExData::FPointIO> PointIO;
		TSharedPtr<FProcessor> Processor;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};

	class FOutputDelaunayUrquhartSites final : public PCGExMT::FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FOutputDelaunayUrquhartSites)

		FOutputDelaunayUrquhartSites(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		                             const TSharedPtr<FProcessor>& InProcessor) :
			FTask(),
			PointIO(InPointIO),
			Processor(InProcessor)
		{
		}

		const TSharedPtr<PCGExData::FPointIO> PointIO;
		TSharedPtr<FProcessor> Processor;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};
}
