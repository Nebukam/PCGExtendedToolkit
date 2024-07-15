// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Async Priority"))
enum class EPCGExAsyncPriority : uint8
{
	Blocking UMETA(DisplayName = "Blocking", ToolTip="Position component."),
	Highest UMETA(DisplayName = "Highest", ToolTip="Position component."),
	High UMETA(DisplayName = "High", ToolTip="Position component."),
	Normal UMETA(DisplayName = "Normal", ToolTip="Position component."),
	Low UMETA(DisplayName = "Low", ToolTip="Position component."),
	Lowest UMETA(DisplayName = "Lowest", ToolTip="Position component."),
};


UCLASS(DefaultConfig, config = Editor, defaultconfig)
class PCGEXTENDEDTOOLKIT_API UPCGExGlobalSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, config, Category = "Performance|Cluster", meta=(ClampMin=1))
	int32 SmallClusterSize = 256;

	UPROPERTY(EditAnywhere, config, Category = "Performance|Cluster", meta=(ClampMin=1))
	int32 ClusterDefaultBatchIterations = 256;
	int32 GetClusterBatchIteration(const int32 In = -1) const { return In <= -1 ? ClusterDefaultBatchIterations : In; }

	/** Allow caching of clusters */
	UPROPERTY(EditAnywhere, config, Category = "Performance|Cluster")
	bool bCacheClusters = true;

	/** Default value for new nodes (Editable per-node in the Graph Output Settings) */
	UPROPERTY(EditAnywhere, config, Category = "Performance|Cluster", meta=(EditCondition="bCacheClusters"))
	bool bDefaultBuildAndCacheClusters = true;

	/** Default value for new nodes (Editable per-node in the Graph Output Settings) */
	UPROPERTY(EditAnywhere, config, Category = "Performance|Cluster", meta=(EditCondition="bDefaultBuildAndCacheClusters&&bCacheClusters"))
	bool bDefaultCacheExpandedClusters = true;


	UPROPERTY(EditAnywhere, config, Category = "Performance|Points", meta=(ClampMin=1))
	int32 SmallPointsSize = 256;
	bool IsSmallPointSize(const int32 InNum) const { return InNum <= SmallPointsSize; }

	UPROPERTY(EditAnywhere, config, Category = "Performance|Points", meta=(ClampMin=1))
	int32 PointsDefaultBatchIterations = 256;
	int32 GetPointsBatchIteration(const int32 In = -1) const { return In <= -1 ? PointsDefaultBatchIterations : In; }

	UPROPERTY(EditAnywhere, config, Category = "Performance|Async")
	EPCGExAsyncPriority DefaultWorkPriority = EPCGExAsyncPriority::Normal;
	
	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorDebug = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorMisc = FLinearColor(1.000000, 0.591295, 0.282534, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorMiscWrite = FLinearColor(1.000000, 0.316174, 0.000000, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorMiscAdd = FLinearColor(0.000000, 1.000000, 0.298310, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorMiscRemove = FLinearColor(0.05, 0.01, 0.01, 1.000000);


	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorSampler = FLinearColor(1.000000, 0.000000, 0.147106, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorSamplerNeighbor = FLinearColor(0.447917, 0.000000, 0.065891, 1.000000);


	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorClusterGen = FLinearColor(0.000000, 0.318537, 1.000000, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorCluster = FLinearColor(0.000000, 0.615363, 1.000000, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorProbe = FLinearColor(0.171875, 0.681472, 1.000000, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorSocketState = FLinearColor(0.000000, 0.249991, 0.406250, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorPathfinding = FLinearColor(0.000000, 1.000000, 0.670588, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorHeuristics = FLinearColor(0.243896, 0.578125, 0.371500, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorHeuristicsAtt = FLinearColor(0.497929, 0.515625, 0.246587, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorClusterFilter = FLinearColor(0.351486, 0.744792, 0.647392, 1.000000);


	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorEdge = FLinearColor(0.000000, 0.670117, 0.760417, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorClusterState = FLinearColor(0.000000, 0.249991, 0.406250, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorPath = FLinearColor(0.000000, 0.239583, 0.160662, 1.000000);


	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorFilterHub = FLinearColor(0.226841, 1.000000, 0.000000, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorFilter = FLinearColor(0.312910, 0.744792, 0.186198, 1.000000);


	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorPrimitives = FLinearColor(0.000000, 0.065291, 1.000000, 1.000000);

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorTransform = FLinearColor(1.000000, 0.000000, 0.185865, 1.000000);
};
