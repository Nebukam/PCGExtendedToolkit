// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.generated.h"

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Async Priority")--E*/)
enum class EPCGExAsyncPriority : uint8
{
	Blocking = 0 UMETA(DisplayName = "Blocking", ToolTip="Position component."),
	Highest  = 1 UMETA(DisplayName = "Highest", ToolTip="Position component."),
	High     = 2 UMETA(DisplayName = "High", ToolTip="Position component."),
	Normal   = 3 UMETA(DisplayName = "Normal", ToolTip="Position component."),
	Low      = 4 UMETA(DisplayName = "Low", ToolTip="Position component."),
	Lowest   = 5 UMETA(DisplayName = "Lowest", ToolTip="Position component."),
	Default  = 6 UMETA(DisplayName = "Default", ToolTip="Position component."),
};

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Data Blending Type (With Defaults)")--E*/)
enum class EPCGExDataBlendingTypeDefault : uint8
{
	Default          = 100 UMETA(DisplayName = "Default", ToolTip="Use the node' default"),
	None             = 0 UMETA(DisplayName = "None", ToolTip="No blending is applied, keep the original value."),
	Average          = 1 UMETA(DisplayName = "Average", ToolTip="Average all sampled values."),
	Weight           = 2 UMETA(DisplayName = "Weight", ToolTip="Weights based on distance to blend targets. If the results are unexpected, try 'Lerp' instead"),
	Min              = 3 UMETA(DisplayName = "Min", ToolTip="Component-wise MIN operation"),
	Max              = 4 UMETA(DisplayName = "Max", ToolTip="Component-wise MAX operation"),
	Copy             = 5 UMETA(DisplayName = "Copy", ToolTip = "Copy incoming data"),
	Sum              = 6 UMETA(DisplayName = "Sum", ToolTip = "Sum"),
	WeightedSum      = 7 UMETA(DisplayName = "Weighted Sum", ToolTip = "Sum of all the data, weighted"),
	Lerp             = 8 UMETA(DisplayName = "Lerp", ToolTip="Uses weight as lerp. If the results are unexpected, try 'Weight' instead."),
	Subtract         = 9 UMETA(DisplayName = "Subtract", ToolTip="Subtract."),
	UnsignedMin      = 10 UMETA(DisplayName = "Unsigned Min", ToolTip="Component-wise MIN on unsigned value, but keeps the sign on written data."),
	UnsignedMax      = 11 UMETA(DisplayName = "Unsigned Max", ToolTip="Component-wise MAX on unsigned value, but keeps the sign on written data."),
	AbsoluteMin      = 12 UMETA(DisplayName = "Unsigned Min", ToolTip="Component-wise MIN on unsigned value, but keeps the sign on written data."),
	AbsoluteMax      = 13 UMETA(DisplayName = "Unsigned Max", ToolTip="Component-wise MAX on unsigned value, but keeps the sign on written data."),
	WeightedSubtract = 14 UMETA(DisplayName = "Weighted Subtract", ToolTip="Substraction of all the data, weighted"),
	CopyOther        = 15 UMETA(DisplayName = "Copy (Other)", ToolTip="Same as copy, but copy the other value"),
};

UCLASS(DefaultConfig, config = Editor, defaultconfig)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExGlobalSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, config, Category = "Performance|Cluster", meta=(ClampMin=1))
	int32 SmallClusterSize = 256;

	UPROPERTY(EditAnywhere, config, Category = "Performance|Cluster", meta=(ClampMin=1))
	int32 ClusterDefaultBatchChunkSize = 512;
	int32 GetClusterBatchChunkSize(const int32 In = -1) const { return In <= -1 ? ClusterDefaultBatchChunkSize : In; }

	/** Allow caching of clusters */
	UPROPERTY(EditAnywhere, config, Category = "Performance|Cluster")
	bool bCacheClusters = true;

	/** Default value for new nodes (Editable per-node in the Graph Output Settings) */
	UPROPERTY(EditAnywhere, config, Category = "Performance|Cluster", meta=(EditCondition="bCacheClusters"))
	bool bDefaultBuildAndCacheClusters = true;

	/** Default value for new nodes (Editable per-node in the Graph Output Settings) */
	UPROPERTY(EditAnywhere, config, Category = "Performance|Cluster", meta=(EditCondition="bDefaultBuildAndCacheClusters&&bCacheClusters"))
	bool bDefaultCacheExpandedClusters = false;


	UPROPERTY(EditAnywhere, config, Category = "Performance|Points", meta=(ClampMin=1))
	int32 SmallPointsSize = 256;
	bool IsSmallPointSize(const int32 InNum) const { return InNum <= SmallPointsSize; }

	UPROPERTY(EditAnywhere, config, Category = "Performance|Points", meta=(ClampMin=1))
	int32 PointsDefaultBatchChunkSize = 512;
	int32 GetPointsBatchChunkSize(const int32 In = -1) const { return In <= -1 ? PointsDefaultBatchChunkSize : In; }

	UPROPERTY(EditAnywhere, config, Category = "Performance|Async")
	EPCGExAsyncPriority DefaultWorkPriority = EPCGExAsyncPriority::Normal;
	EPCGExAsyncPriority GetDefaultWorkPriority() const { return DefaultWorkPriority == EPCGExAsyncPriority::Default ? EPCGExAsyncPriority::Normal : DefaultWorkPriority; }

	UPROPERTY(EditAnywhere, config, Category = "Blending|Attribute Types Defaults|Simple Types", meta=(DisplayName="Boolean"))
	EPCGExDataBlendingTypeDefault DefaultBooleanBlendMode = EPCGExDataBlendingTypeDefault::Default;

	UPROPERTY(EditAnywhere, config, Category = "Blending|Attribute Types Defaults|Simple Types", meta=(DisplayName="Float"))
	EPCGExDataBlendingTypeDefault DefaultFloatBlendMode = EPCGExDataBlendingTypeDefault::Default;

	UPROPERTY(EditAnywhere, config, Category = "Blending|Attribute Types Defaults|Simple Types", meta=(DisplayName="Double"))
	EPCGExDataBlendingTypeDefault DefaultDoubleBlendMode = EPCGExDataBlendingTypeDefault::Default;

	UPROPERTY(EditAnywhere, config, Category = "Blending|Attribute Types Defaults|Simple Types", meta=(DisplayName="Integer32"))
	EPCGExDataBlendingTypeDefault DefaultInteger32BlendMode = EPCGExDataBlendingTypeDefault::Default;

	UPROPERTY(EditAnywhere, config, Category = "Blending|Attribute Types Defaults|Simple Types", meta=(DisplayName="Integer64"))
	EPCGExDataBlendingTypeDefault DefaultInteger64BlendMode = EPCGExDataBlendingTypeDefault::Default;

	UPROPERTY(EditAnywhere, config, Category = "Blending|Attribute Types Defaults|Vector Types", meta=(DisplayName="Vector2"))
	EPCGExDataBlendingTypeDefault DefaultVector2BlendMode = EPCGExDataBlendingTypeDefault::Default;

	UPROPERTY(EditAnywhere, config, Category = "Blending|Attribute Types Defaults|Vector Types", meta=(DisplayName="Vector"))
	EPCGExDataBlendingTypeDefault DefaultVectorBlendMode = EPCGExDataBlendingTypeDefault::Default;

	UPROPERTY(EditAnywhere, config, Category = "Blending|Attribute Types Defaults|Vector Types", meta=(DisplayName="Vector4"))
	EPCGExDataBlendingTypeDefault DefaultVector4BlendMode = EPCGExDataBlendingTypeDefault::Default;

	UPROPERTY(EditAnywhere, config, Category = "Blending|Attribute Types Defaults|Complex Types", meta=(DisplayName="Quaternion"))
	EPCGExDataBlendingTypeDefault DefaultQuaternionBlendMode = EPCGExDataBlendingTypeDefault::Default;

	UPROPERTY(EditAnywhere, config, Category = "Blending|Attribute Types Defaults|Complex Types", meta=(DisplayName="Transform"))
	EPCGExDataBlendingTypeDefault DefaultTransformBlendMode = EPCGExDataBlendingTypeDefault::Default;

	UPROPERTY(EditAnywhere, config, Category = "Blending|Attribute Types Defaults|Complex Types", meta=(DisplayName="Rotator"))
	EPCGExDataBlendingTypeDefault DefaultRotatorBlendMode = EPCGExDataBlendingTypeDefault::Default;

	UPROPERTY(EditAnywhere, config, Category = "Blending|Attribute Types Defaults|Text Types", meta=(DisplayName="String"))
	EPCGExDataBlendingTypeDefault DefaultStringBlendMode = EPCGExDataBlendingTypeDefault::Copy;

	UPROPERTY(EditAnywhere, config, Category = "Blending|Attribute Types Defaults|Text Types", meta=(DisplayName="Name"))
	EPCGExDataBlendingTypeDefault DefaultNameBlendMode = EPCGExDataBlendingTypeDefault::Copy;

	UPROPERTY(EditAnywhere, config, Category = "Blending|Attribute Types Defaults|Soft Paths Types", meta=(DisplayName="SoftObjectPath"))
	EPCGExDataBlendingTypeDefault DefaultSoftObjectPathBlendMode = EPCGExDataBlendingTypeDefault::Copy;

	UPROPERTY(EditAnywhere, config, Category = "Blending|Attribute Types Defaults|Soft Paths Types", meta=(DisplayName="SoftClassPath"))
	EPCGExDataBlendingTypeDefault DefaultSoftClassPathBlendMode = EPCGExDataBlendingTypeDefault::Copy;


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
	FLinearColor NodeColorTopology = FLinearColor(0.447917, 0.000000, 0.065891, 1.000000);

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
	FLinearColor NodeColorHeuristicsFeedback = FLinearColor(1.000000, 0.316174, 0.000000, 1.000000);

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

	UPROPERTY(EditAnywhere, config, Category = "Node Colors")
	FLinearColor NodeColorShapeBuilder = FLinearColor(1.000000, 0.000000, 0.185865, 1.000000);
};
