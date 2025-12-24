// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExPointsProcessor.h"
#include "Clusters/PCGExClusterCommon.h"
#include "Graphs/PCGExGraphDetails.h"
#include "Math/PCGExProjectionDetails.h"
#include "Math/Geo/PCGExGeo.h"
#include "Math/Geo/PCGExVoronoi.h"
#include "Utils/PCGValueRange.h"

#include "PCGExBuildVoronoiGraph2D.generated.h"

namespace PCGExGraphs
{
	class FGraphBuilder;
}

namespace PCGExData
{
	template <typename T>
	class TBuffer;
}

/** Parameters for conducting a sweep with a specified shape against the physical world. */
USTRUCT(BlueprintType)
struct FPCGExVoronoiSitesOutputDetails
{
	GENERATED_BODY()

	FPCGExVoronoiSitesOutputDetails() = default;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteInfluencesCount = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta = (PCG_Overridable, EditCondition="bWriteInfluencesCount"))
	FName InfluencesCountAttributeName = "InfluencesCount";

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteMinRadius = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta = (PCG_Overridable, EditCondition="bWriteMinRadius"))
	FName MinRadiusAttributeName = "MinRadius";

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteMaxRadius = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta = (PCG_Overridable, EditCondition="bWriteMaxRadius"))
	FName MaxRadiusAttributeName = "MaxRadius";

	bool Validate(FPCGExContext* InContext) const;

	TArray<FVector> Locations;
	TArray<int32> Influences;

	void Init(const TSharedPtr<PCGExData::FFacade>& InSiteFacade);
	void AddInfluence(const int32 SiteIndex, const FVector& SitePosition);
	void Output(const int32 SiteIndex);

protected:
	bool bWantsDist = false;
	TConstPCGValueRange<FTransform> InTransforms;

	TSharedPtr<TArray<double>> MinRadius;
	TSharedPtr<TArray<double>> MaxRadius;

	TSharedPtr<PCGExData::TBuffer<double>> MinRadiusWriter;
	TSharedPtr<PCGExData::TBuffer<double>> MaxRadiusWriter;
	TSharedPtr<PCGExData::TBuffer<int32>> InfluenceCountWriter;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="clusters/diagrams/voronoi-2d"))
class UPCGExBuildVoronoiGraph2DSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildVoronoiGraph2D, "Cluster : Voronoi 2D", "Create a 2D Voronoi graph for each input dataset.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(ClusterGenerator); }
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual FName GetMainOutputPin() const override { return PCGExClusters::Labels::OutputVerticesLabel; }
	//~End UPCGExPointsProcessorSettings

	/** Method used to find Voronoi cell location */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExCellCenter Method = EPCGExCellCenter::Centroid;

	/** Bounds used for point pruning & balanced centroid. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double ExpandBounds = 100;

	/** Prune points outside bounds */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Method == EPCGExCellCenter::Circumcenter"))
	bool bPruneOutOfBounds = false;

	/** Mark points & edges that lie on the hull */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bMarkHull = false;

	/** Name of the attribute to output the Hull boolean to. True if point is on the hull, otherwise false. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bMarkHull"))
	FName HullAttributeName = "bIsOnHull";

	/** When true, edges that have at least a point on the Hull as marked as being on the hull. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bMarkEdgeOnTouch = false;

	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails;

	/** Graph & Edges output properties. Only available if bPruneOutsideBounds as it otherwise generates a complete graph. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails = FPCGExGraphBuilderDetails(EPCGExMinimalAxis::X);

	/** Whether to output updated sites */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta = (PCG_Overridable))
	bool bOutputSites = true;


	/** If enabled, sites that belong to an removed (out-of-bound) cell will be removed from the output. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta = (PCG_Overridable, EditCondition="bOutputSites && bPruneOutOfBounds"))
	bool bPruneOpenSites = true;

	/** Flag sites belonging to an open cell with a boolean attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta = (PCG_Overridable, EditCondition="bOutputSites && bPruneOutOfBounds && !bPruneOpenSites"))
	FName OpenSiteFlag = "OpenSite";

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta = (PCG_Overridable, EditCondition="bOutputSites", ShowOnlyInnerProperties))
	FPCGExVoronoiSitesOutputDetails SitesOutputDetails;

private:
	friend class FPCGExBuildVoronoiGraph2DElement;
};

struct FPCGExBuildVoronoiGraph2DContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExBuildVoronoiGraph2DElement;

	TSharedPtr<PCGExData::FPointIOCollection> SitesOutput;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExBuildVoronoiGraph2DElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(BuildVoronoiGraph2D)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExBuildVoronoiGraph2D
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExBuildVoronoiGraph2DContext, UPCGExBuildVoronoiGraph2DSettings>
	{
	protected:
		FPCGExGeo2DProjectionDetails ProjectionDetails;

		TBitArray<> WithinBounds;
		TBitArray<> IsVtxValid;

		TArray<FVector> SitesPositions;

		TSharedPtr<TArray<int32>> OutputIndices;
		TSharedPtr<PCGExMath::Geo::TVoronoi2> Voronoi;
		TSharedPtr<PCGExGraphs::FGraphBuilder> GraphBuilder;

		TSharedPtr<PCGExData::FFacade> SiteDataFacade;
		TSharedPtr<PCGExData::TBuffer<bool>> HullMarkPointWriter;
		TSharedPtr<PCGExData::TBuffer<bool>> OpenSiteWriter;

		FPCGExVoronoiSitesOutputDetails SitesOutputDetails;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;

		virtual void CompleteWork() override;
		virtual void Write() override;
		virtual void Output() override;
	};
}
