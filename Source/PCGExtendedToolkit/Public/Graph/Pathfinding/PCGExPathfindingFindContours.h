// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Geometry/PCGExGeo.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExPathfindingFindContours.generated.h"

namespace PCGExFindContours
{
	class FProcessor;
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExFindContoursSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindContours, "Pathfinding : Find Contours", "Attempts to find a closed contour of connected edges around seed points.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorPathfinding; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

	/** Drive how a seed selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExNodeSelectionSettings SeedPicking;

	/** Drives how the seed nodes are selected within the graph. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExClusterSearchOrientationMode OrientationMode = EPCGExClusterSearchOrientationMode::CW;

	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionSettings ProjectionSettings;

	/** Use a seed attribute value to tag output paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	bool bUseSeedAttributeToTagPath;

	/** Which Seed attribute to use as tag. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="bUseSeedAttributeToTagPath"))
	FPCGAttributePropertyInputSelector SeedTagAttribute;

	/** Which Seed attributes to forward on paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExForwardSettings SeedForwardAttributes;

	/** Whether or not to search for closest node using an octree. Depending on your dataset, enabling this may be either much faster, or much slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance")
	bool bUseOctreeSearch = false;

private:
	friend class FPCGExFindContoursElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFindContoursContext final : public FPCGExEdgesProcessorContext
{
	friend class FPCGExFindContoursElement;
	friend class FPCGExCreateBridgeTask;

	virtual ~FPCGExFindContoursContext() override;

	PCGExData::FPointIO* Seeds;
	TArray<FVector> ProjectedSeeds;

	PCGExData::FPointIOCollection* Paths;

	PCGEx::FLocalToStringGetter* SeedTagGetter;
	PCGExDataBlending::FDataForwardHandler* SeedForwardHandler;

	bool TryFindContours(PCGExData::FPointIO* PointIO, const int32 SeedIndex, const PCGExFindContours::FProcessor* ClusterProcessor);
};

class PCGEXTENDEDTOOLKIT_API FPCGExFindContoursElement final : public FPCGExEdgesProcessorElement
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

namespace PCGExFindContours
{
	class FProcessor final : public PCGExClusterMT::FClusterProcessor
	{
		friend struct FPCGExFindContoursContext;

	protected:
		FPCGExGeo2DProjectionSettings ProjectionSettings;
		PCGExCluster::FClusterProjection* ClusterProjection = nullptr;

	public:
		FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
			FClusterProcessor(InVtx, InEdges)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void CompleteWork() override;

		virtual void ProcessSingleNode(PCGExCluster::FNode& Node) override;

		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
	};

	class PCGEXTENDEDTOOLKIT_API FPCGExFindContourTask final : public PCGExMT::FPCGExTask
	{
	public:
		FPCGExFindContourTask(PCGExData::FPointIO* InPointIO,
		                      FProcessor* InClusterProcessor) :
			FPCGExTask(InPointIO),
			ClusterProcessor(InClusterProcessor)
		{
		}

		FProcessor* ClusterProcessor = nullptr;

		virtual bool ExecuteTask() override;
	};
}
