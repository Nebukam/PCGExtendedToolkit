// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"


#include "Geometry/PCGExGeo.h"
#include "Geometry/PCGExGeoVoronoi.h"

#include "PCGExBuildVoronoiGraph.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBuildVoronoiGraphSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildVoronoiGraph, "Cluster : Voronoi 3D", "Create a 3D Voronoi graph for each input dataset.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterGen; }
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual FName GetMainOutputPin() const override { return PCGExGraph::OutputVerticesLabel; }
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Method used to find Voronoi cell location */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExCellCenter Method = EPCGExCellCenter::Centroid;

	/** Bounds used for point pruning & balanced centroid. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double ExpandBounds = 100;

	/** Prune points outside bounds */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Method==EPCGExCellCenter::Circumcenter"))
	bool bPruneOutOfBounds = false;

	/** Mark points & edges that lie on the hull */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bMarkHull = true;

	/** Name of the attribute to output the Hull boolean to. True if point is on the hull, otherwise false. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bMarkHull"))
	FName HullAttributeName = "bIsOnHull";

	/** When true, edges that have at least a point on the Hull as marked as being on the hull. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bMarkEdgeOnTouch = false;

	/** Graph & Edges output properties. Only available if bPruneOutsideBounds as it otherwise generates a complete graph. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails = FPCGExGraphBuilderDetails();

	// TODO : Output modified/pruned sites to ensure we can find contours even tho the centroid method is not canon

private:
	friend class FPCGExBuildVoronoiGraphElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBuildVoronoiGraphContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExBuildVoronoiGraphElement;

	TSharedPtr<PCGExData::FPointIOCollection> SitesOutput;
};


class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBuildVoronoiGraphElement final : public FPCGExPointsProcessorElement
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

namespace PCGExBuildVoronoi
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExBuildVoronoiGraphContext, UPCGExBuildVoronoiGraphSettings>
	{
	protected:
		TUniquePtr<PCGExGeo::TVoronoi3> Voronoi;
		TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder;

		PCGExData::TBuffer<bool>* HullMarkPointWriter = nullptr;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
