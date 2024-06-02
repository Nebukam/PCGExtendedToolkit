// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Geometry/PCGExGeo.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExPathfindingFindContours.generated.h"

namespace PCGExContours
{
	struct PCGEXTENDEDTOOLKIT_API FCandidate
	{
		explicit FCandidate(const int32 InNodeIndex)
			: NodeIndex(InNodeIndex)
		{
		}

		int32 NodeIndex;
		double Distance = TNumericLimits<double>::Max();
		double Dot = -1;
	};
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExFindContoursSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExFindContoursSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindContours, "Pathfinding : Find Contours", "Attempts to find a closed contour of connected edges around seed points.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExEditorSettings>()->NodeColorPathfinding; }
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

private:
	friend class FPCGExFindContoursElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFindContoursContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExFindContoursElement;
	friend class FPCGExCreateBridgeTask;

	virtual ~FPCGExFindContoursContext() override;

	PCGExData::FPointIOCollection* Seeds;
	PCGExData::FPointIOCollection* Paths;

	TArray<PCGEx::FLocalToStringGetter*> SeedTagGetters;
	TArray<PCGExDataBlending::FDataForwardHandler*> SeedForwardHandlers;

	TArray<TArray<FVector>*> ProjectedSeeds;
};

class PCGEXTENDEDTOOLKIT_API FPCGExFindContoursElement : public FPCGExEdgesProcessorElement
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

class PCGEXTENDEDTOOLKIT_API FPCGExFindContourTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExFindContourTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
	                      PCGExCluster::FCluster* InCluster) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		Cluster(InCluster)
	{
	}

	PCGExCluster::FCluster* Cluster = nullptr;

	virtual bool ExecuteTask() override;
};
