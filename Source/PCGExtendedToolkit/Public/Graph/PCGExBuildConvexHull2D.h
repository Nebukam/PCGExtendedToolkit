// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExCustomGraphProcessor.h"
#include "Geometry/PCGExGeo.h"

#include "PCGExBuildConvexHull2D.generated.h"

namespace PCGExGeo
{
	class TDelaunay2;
}

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExBuildConvexHull2DSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildConvexHull2D, "Graph : Convex Hull 2D", "Create a 2D Convex Hull triangulation for each input dataset. Deprecated as of 5.4; use Find Convex Hull 2D instead.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorGraph; }
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
	/** Mark points & edges that lie on the hull */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bMarkHull = true;

	/** Name of the attribute to output the Hull boolean to. True if point is on the hull, otherwise false. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bMarkHull"))
	FName HullAttributeName = "bIsOnHull";

	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionSettings ProjectionSettings;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Graph Output Settings"))
	FPCGExGraphBuilderSettings GraphBuilderSettings = FPCGExGraphBuilderSettings(true);

private:
	friend class FPCGExBuildConvexHull2DElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBuildConvexHull2DContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExBuildConvexHull2DElement;

	virtual ~FPCGExBuildConvexHull2DContext() override;

	PCGExData::FPointIOCollection* PathsIO;

	void BuildPath(const PCGExGraph::FGraphBuilder* GraphBuilder) const;
};


class PCGEXTENDEDTOOLKIT_API FPCGExBuildConvexHull2DElement final : public FPCGExPointsProcessorElement
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

namespace PCGExConvexHull2D
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
	protected:
		FPCGExGeo2DProjectionSettings ProjectionSettings;

		PCGExGeo::TDelaunay2* Delaunay = nullptr;
		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;

		TArray<uint64> Edges;

		PCGEx::TFAttributeWriter<bool>* HullMarkPointWriter = nullptr;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
