// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExCustomGraphProcessor.h"
#include "Geometry/PCGExGeo.h"

#include "PCGExBuildVoronoiGraph2D.generated.h"

namespace PCGExGeo
{
	class TVoronoi2;
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExBuildVoronoiGraph2DSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildVoronoiGraph2D, "Graph : Voronoi 2D", "Create a 2D Voronoi graph for each input dataset.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorGraphGen; }
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual FName GetMainOutputLabel() const override;
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Method used to find Voronoi cell location */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExCellCenter Method = EPCGExCellCenter::Centroid;

	/** Prune points and cell outside bounds (computed based on input vertices + optional extension)*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double ExpandBounds = 100;

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
	FPCGExGeo2DProjectionSettings ProjectionSettings;

	/** Graph & Edges output properties. Only available if bPruneOutsideBounds as it otherwise generates a complete graph. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditConditionHides, DisplayName="Graph Output Settings"))
	FPCGExGraphBuilderSettings GraphBuilderSettings = FPCGExGraphBuilderSettings(false);

private:
	friend class FPCGExBuildVoronoiGraph2DElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBuildVoronoiGraph2DContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExBuildVoronoiGraph2DElement;

	virtual ~FPCGExBuildVoronoiGraph2DContext() override;
};


class PCGEXTENDEDTOOLKIT_API FPCGExBuildVoronoiGraph2DElement final : public FPCGExPointsProcessorElement
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

namespace PCGExBuildVoronoi2D
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{

	protected:
		FPCGExGeo2DProjectionSettings ProjectionSettings;
		
		PCGExGeo::TVoronoi2* Voronoi = nullptr;
		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;

		PCGEx::TFAttributeWriter<bool>* HullMarkPointWriter = nullptr;
		
	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints);
		virtual ~FProcessor() override;

		virtual bool Process(FPCGExAsyncManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
		
	};
}