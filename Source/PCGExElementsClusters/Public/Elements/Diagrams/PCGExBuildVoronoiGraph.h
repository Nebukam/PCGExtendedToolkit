// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExPointsProcessor.h"
#include "Graphs/PCGExGraphDetails.h"
#include "Math/Geo/PCGExGeo.h"
#include "Clusters/PCGExClusterCommon.h"

#include "PCGExBuildVoronoiGraph.generated.h"

namespace PCGExGraphs
{
	class FGraphBuilder;
}

namespace PCGExMath
{
	namespace Geo
	{
		class TVoronoi3;
	}

	class TVoronoi3;
}

namespace PCGExData
{
	template <typename T>
	class TBuffer;
}

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="clusters/diagrams/voronoi-3d"))
class UPCGExBuildVoronoiGraphSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildVoronoiGraph, "Cluster : Voronoi 3D", "Create a 3D Voronoi graph for each input dataset.");
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

	/** Graph & Edges output properties. Only available if bPruneOutsideBounds as it otherwise generates a complete graph. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails = FPCGExGraphBuilderDetails(EPCGExMinimalAxis::X);

	// TODO : Output modified/pruned sites to ensure we can find contours even tho the centroid method is not canon

private:
	friend class FPCGExBuildVoronoiGraphElement;
};

struct FPCGExBuildVoronoiGraphContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExBuildVoronoiGraphElement;

	TSharedPtr<PCGExData::FPointIOCollection> SitesOutput;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExBuildVoronoiGraphElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(BuildVoronoiGraph)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExBuildVoronoiGraph
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExBuildVoronoiGraphContext, UPCGExBuildVoronoiGraphSettings>
	{
	protected:
		TSharedPtr<TArray<int32>> OutputIndices;
		TSharedPtr<PCGExMath::Geo::TVoronoi3> Voronoi;
		TSharedPtr<PCGExGraphs::FGraphBuilder> GraphBuilder;

		PCGExData::TBuffer<bool>* HullMarkPointWriter = nullptr;

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
