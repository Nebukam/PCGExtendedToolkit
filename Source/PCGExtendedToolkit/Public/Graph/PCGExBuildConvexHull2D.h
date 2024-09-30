// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"


#include "Geometry/PCGExGeo.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "PCGExBuildConvexHull2D.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBuildConvexHull2DSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildConvexHull2D, "Cluster : Convex Hull 2D", "Create a 2D Convex Hull triangulation for each input dataset. Deprecated as of 5.4; use Find Convex Hull 2D instead.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorCluster; }
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
	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails = FPCGExGraphBuilderDetails();

private:
	friend class FPCGExBuildConvexHull2DElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBuildConvexHull2DContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExBuildConvexHull2DElement;

	TSharedPtr<PCGExData::FPointIOCollection> PathsIO;

	void BuildPath(const PCGExGraph::FGraphBuilder* GraphBuilder) const;
};


class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBuildConvexHull2DElement final : public FPCGExPointsProcessorElement
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

namespace PCGExConvexHull2D
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExBuildConvexHull2DContext, UPCGExBuildConvexHull2DSettings>
	{
	protected:
		FPCGExGeo2DProjectionDetails ProjectionDetails;

		TUniquePtr<PCGExGeo::TDelaunay2> Delaunay;
		TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder;

		TArray<uint64> Edges;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
