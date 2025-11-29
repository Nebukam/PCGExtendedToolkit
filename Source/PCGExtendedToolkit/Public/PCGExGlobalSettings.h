// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "Engine/EngineTypes.h"
#include "Engine/DeveloperSettings.h"

#include "PCGExGlobalSettings.generated.h"

class UPCGPin;

UENUM()
enum class EPCGExDataBlendingTypeDefault : uint8
{
	Default          = 100 UMETA(DisplayName = "Default", ToolTip="Use the node' default"),
	None             = 0 UMETA(DisplayName = "None", ToolTip="No blending is applied, keep the original value."),
	Average          = 1 UMETA(DisplayName = "Average", ToolTip="Average all sampled values."),
	Weight           = 2 UMETA(DisplayName = "Weight", ToolTip="Weights based on distance to blend targets. If the results are unexpected, try 'Lerp' instead"),
	Min              = 3 UMETA(DisplayName = "Min", ToolTip="Component-wise MIN operation"),
	Max              = 4 UMETA(DisplayName = "Max", ToolTip="Component-wise MAX operation"),
	Copy             = 5 UMETA(DisplayName = "Copy (Target)", ToolTip = "Copy target data (second value)"),
	Sum              = 6 UMETA(DisplayName = "Sum", ToolTip = "Sum"),
	WeightedSum      = 7 UMETA(DisplayName = "Weighted Sum", ToolTip = "Sum of all the data, weighted"),
	Lerp             = 8 UMETA(DisplayName = "Lerp", ToolTip="Uses weight as lerp. If the results are unexpected, try 'Weight' instead."),
	Subtract         = 9 UMETA(DisplayName = "Subtract", ToolTip="Subtract."),
	UnsignedMin      = 10 UMETA(DisplayName = "Unsigned Min", ToolTip="Component-wise MIN on unsigned value, but keeps the sign on written data."),
	UnsignedMax      = 11 UMETA(DisplayName = "Unsigned Max", ToolTip="Component-wise MAX on unsigned value, but keeps the sign on written data."),
	AbsoluteMin      = 12 UMETA(DisplayName = "Unsigned Min", ToolTip="Component-wise MIN on unsigned value, but keeps the sign on written data."),
	AbsoluteMax      = 13 UMETA(DisplayName = "Unsigned Max", ToolTip="Component-wise MAX on unsigned value, but keeps the sign on written data."),
	WeightedSubtract = 14 UMETA(DisplayName = "Weighted Subtract", ToolTip="Substraction of all the data, weighted"),
	CopyOther        = 15 UMETA(DisplayName = "Copy (Source)", ToolTip="Copy source data (first value)"),
	Hash             = 16 UMETA(DisplayName = "Hash", ToolTip="Combine the values into a hash"),
	UnsignedHash     = 17 UMETA(DisplayName = "Hash (Unsigned)", ToolTip="Combine the values into a hash but sort the values first to create an order-independent hash."),
};

namespace PCGEx
{
	struct FPinInfos
	{
		FName Icon = NAME_None;
		FText Tooltip = FText();

		FPinInfos() = default;

		FPinInfos(const FName InIcon, const FString& InTooltip)
			: Icon(InIcon), Tooltip(FText::FromString(InTooltip))
		{
		}

		~FPinInfos() = default;
	};
}

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="PCGEx", Description="Configure PCG Extended Toolkit settings"))
class PCGEXTENDEDTOOLKIT_API UPCGExGlobalSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** World "Up" vector used as default up internally */
	UPROPERTY(EditAnywhere, config, Category = "Defaults")
	FVector WorldUp = FVector::UpVector;

	/** World "Forward" vector used as default up internally (Custom X direction). Should be in relation to the specific Up (Z) axis. */
	UPROPERTY(EditAnywhere, config, Category = "Defaults")
	FVector WorldForward = FVector::ForwardVector;

	/** Value applied by default to node caching when `Default` is selected -- note that some nodes may stop working as expected when working with cached data.*/
	UPROPERTY(EditAnywhere, config, Category = "Performance|Defaults")
	bool bDefaultCacheNodeOutput = true;

	/** Value applied by default to node caching when `Default` is selected*/
	UPROPERTY(EditAnywhere, config, Category = "Performance|Defaults")
	bool bDefaultScopedAttributeGet = true;

	/** Value applied by default to node bulk init data when `Default` is selected. */
	UPROPERTY(EditAnywhere, config, Category = "Performance|Defaults")
	bool bBulkInitData = false;

	UPROPERTY(EditAnywhere, config, Category = "Performance|Cluster")
	bool bUseDelaunator = true;

	UPROPERTY(EditAnywhere, config, Category = "Performance|Cluster", meta=(ClampMin=1))
	int32 SmallClusterSize = 512;

	UPROPERTY(EditAnywhere, config, Category = "Performance|Cluster", meta=(ClampMin=1))
	int32 ClusterDefaultBatchChunkSize = 512;
	int32 GetClusterBatchChunkSize(const int32 In = -1) const { return In <= -1 ? ClusterDefaultBatchChunkSize : In; }

	UPROPERTY(EditAnywhere, config, Category = "Performance|Cluster")
	bool bDefaultScopedIndexLookupBuild = false;

	/** Allow caching of clusters so they don't have to be rebuilt every time data changes hands */
	UPROPERTY(EditAnywhere, config, Category = "Performance|Cluster")
	bool bCacheClusters = true;

	/** Default value for new nodes (Editable per-node in the Graph Output Settings) */
	UPROPERTY(EditAnywhere, config, Category = "Performance|Cluster", meta=(EditCondition="bCacheClusters"))
	bool bDefaultBuildAndCacheClusters = true;

	UPROPERTY(EditAnywhere, config, Category = "Performance|Points", meta=(ClampMin=1))
	int32 SmallPointsSize = 1024;
	bool IsSmallPointSize(const int32 InNum) const { return InNum <= SmallPointsSize; }

	UPROPERTY(EditAnywhere, config, Category = "Performance|Points", meta=(ClampMin=1))
	int32 PointsDefaultBatchChunkSize = 1024;
	int32 GetPointsBatchChunkSize(const int32 In = -1) const { return In <= -1 ? PointsDefaultBatchChunkSize : In; }

	UPROPERTY(EditAnywhere, config, Category = "Performance|Async")
	EPCGExAsyncPriority DefaultWorkPriority = EPCGExAsyncPriority::BackgroundNormal;
	EPCGExAsyncPriority GetDefaultWorkPriority() const { return DefaultWorkPriority == EPCGExAsyncPriority::Default ? EPCGExAsyncPriority::BackgroundNormal : DefaultWorkPriority; }

	/** If enabled, debug generated by PCG will not be transient. (Pre-5.6 behavior) (Requires restarting the editor.)*/
	UPROPERTY(EditAnywhere, config, Category = "Debug")
	bool bPersistentDebug = false;

	/** If enabled, code will assert when attempting to schedule zero task. Requires a debugguer attached to the editor, otherwise will crash.  */
	UPROPERTY(EditAnywhere, config, Category = "Debug")
	bool bAssertOnEmptyThread = false;

	/** Disable collision on new entries */
	UPROPERTY(EditAnywhere, config, Category = "Collections")
	bool bDisableCollisionByDefault = true;

#pragma region Blendmodes

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

#pragma endregion

#pragma region Node Colors

	/**If enabled, non-required pin that are disconnected will be toned down. Helps reduce the confusion regarding which pin matters; but may, on the contrary, be perceived as being more "noisy" in a graph. */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics")
	bool bToneDownOptionalPins = true;

	/** If enabled, will use native node colors where relevant. I.e filters, spawners, etc. in order to stay as close as possible from the vanilla color semantics. */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics")
	bool bUseNativeColorsIfPossible = true;

	///// NODES

	/** Color associated with constants & nodes that output constant values */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics")
	FLinearColor ColorConstant = FLinearColor(0.2, 0.2, 0.2, 1.0);

	/** Color associated with debug nodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics")
	FLinearColor ColorDebug = FLinearColor(1.0f, 0.0f, 1.0f, 1.0f);

	/** Color associated with misc nodes, that don't really fall in any specific category */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics")
	FLinearColor ColorMisc = FLinearColor(1.000000, 0.591295, 0.282534, 1.000000);

	/** Color associated with misc nodes that usually write new attribute & values to existing data */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics")
	FLinearColor ColorMiscWrite = FLinearColor(1.000000, 0.316174, 0.000000, 1.000000);

	/** Color associated with nodes that generate new data or split existing data into more data */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics")
	FLinearColor ColorMiscAdd = FLinearColor(1.000000, 0.591295, 0.282534, 1.000000);

	/** Color associated with nodes that remove and delete things */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics")
	FLinearColor ColorMiscRemove = FLinearColor(0.05, 0.01, 0.01, 1.000000);

	/** Color associated with nodes that grab attributes and value from external sources */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics")
	FLinearColor ColorSampling = FLinearColor(1.000000, 0.251440, 0.000000, 1.000000);

	/** Color associated with nodes that creates cluster data. */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics")
	FLinearColor ColorClusterGenerator = FLinearColor(0.000000, 0.318537, 1.000000, 1.000000);

	/** Color associated with nodes that do operations on clusters */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics")
	FLinearColor ColorClusterOp = FLinearColor(0.000000, 0.670117, 0.760417, 1.000000);

	/** Color associated with nodes that do pathfinding-like operations on clusters */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics")
	FLinearColor ColorPathfinding = FLinearColor(0.243896, 0.578125, 0.371500, 1.000000);

	/** Color associated with nodes that do operations on path-like data */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics")
	FLinearColor ColorPath = FLinearColor(0.000000, 0.239583, 0.160662, 1.000000);

	/** Color associated with nodes that focus solely on filtering data */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics", meta=(EditCondition="!bUseNativeColorsIfPossible"))
	FLinearColor ColorFilterHub = FLinearColor(0.226841, 1.000000, 0.000000, 1.000000);

	/** Color associated with nodes that focus on spatial transformations */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics")
	FLinearColor ColorTransform = FLinearColor(1.000000, 0.000000, 0.185865, 1.000000);

	FLinearColor WantsColor(FLinearColor InColor) const;

	///// SUBNODES + PINS

	/** Color associated with action subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes")
	FLinearColor ColorAction = FLinearColor(1.000000, 0.592852, 0.105316, 1.000000);

	/** Color associated with blend operations subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes")
	FLinearColor ColorBlendOp = FLinearColor(1.000000, 0.591295, 0.282534, 1.000000);

	/** Color associated with match rules subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes")
	FLinearColor ColorMatchRule = FLinearColor(0.020020, 1.000000, 0.036055, 1.000000);


	/** Color associated with filter (generic) subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes|Filters", meta=(EditCondition="!bUseNativeColorsIfPossible"))
	FLinearColor ColorFilter = FLinearColor(0.312910, 0.744792, 0.186198, 1.000000);

	/** Color associated with filter (points) subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes|Filters", meta=(EditCondition="!bUseNativeColorsIfPossible"))
	FLinearColor ColorFilterPoint = FLinearColor(0.312910, 0.744792, 0.186198, 1.000000);

	/** Color associated with filter (collections) subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes|Filters", meta=(EditCondition="!bUseNativeColorsIfPossible"))
	FLinearColor ColorFilterCollection = FLinearColor(0.312910, 0.744792, 0.186198, 1.000000);

	/** Color associated with filter (cluster) subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes|Filters", meta=(EditCondition="!bUseNativeColorsIfPossible"))
	FLinearColor ColorFilterCluster = FLinearColor(0.351486, 0.744792, 0.647392, 1.000000);

	/** Color associated with filter (vtx) subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes|Filters", meta=(EditCondition="!bUseNativeColorsIfPossible"))
	FLinearColor ColorFilterVtx = FLinearColor(0.351486, 0.744792, 0.647392, 1.000000);

	/** Color associated with filter (edges) subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes|Filters", meta=(EditCondition="!bUseNativeColorsIfPossible"))
	FLinearColor ColorFilterEdge = FLinearColor(0.351486, 0.744792, 0.647392, 1.000000);

	/** Color associated with Vtx Property subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes")
	FLinearColor ColorVtxProperty = FLinearColor(0.000000, 0.284980, 1.000000, 1.000000);

	/** Color associated with Neighbor Sampler subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes")
	FLinearColor ColorNeighborSampler = FLinearColor(1.000000, 0.591295, 0.282534, 1.000000);

	/** Color associated with Fill Control subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes")
	FLinearColor ColorFillControl = FLinearColor(0.312910, 0.744792, 0.186198, 1.000000);

	/** Color associated with Heuristics subnodes. */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes|Heuristics")
	FLinearColor ColorHeuristics = FLinearColor(0.203896, 0.508125, 0.371500, 1.000000);

	/** Color associated with Heuristics subnodes relying on attributes. */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes|Heuristics")
	FLinearColor ColorHeuristicsAttribute = FLinearColor(0.497929, 0.515625, 0.246587, 1.000000);

	/** Color associated with "Feedback" Heuristics subnodes. */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes|Heuristics")
	FLinearColor ColorHeuristicsFeedback = FLinearColor(1.000000, 0.316174, 0.000000, 1.000000);

	/** Color associated with Probes subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes")
	FLinearColor ColorProbe = FLinearColor(0.171875, 0.681472, 1.000000, 1.000000);

	/** Color associated with cluster state (node flags) subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes")
	FLinearColor ColorClusterState = FLinearColor(0.755417, 0.12192, 0.000000, 1.000000);

	/** Color associated with pickers subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes")
	FLinearColor ColorPicker = FLinearColor(1.000000, 0.591295, 0.282534, 1.000000);

	/** Color associated with tex params subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes")
	FLinearColor ColorTexParam = FLinearColor(1.000000, 0.200000, 0.185865, 1.000000);

	/** Color associated with shapes subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes")
	FLinearColor ColorShape = FLinearColor(1.000000, 0.000000, 0.185865, 1.000000);

	/** Color associated with tensors subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes")
	FLinearColor ColorTensor = FLinearColor(0.350314, 1.000000, 0.470501, 1.000000);

	/** Color associated with sort rules subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes")
	FLinearColor ColorSortRule = FLinearColor(1.000000, 0.591295, 0.282534, 1.000000);

	/** Color associated with partition rules subnodes */
	UPROPERTY(EditAnywhere, config, Category = "Colors and Semantics|Subnodes")
	FLinearColor ColorPartitionRule = FLinearColor(1.000000, 0.591295, 0.282534, 1.000000);


#pragma endregion

	bool GetPinExtraIcon(const UPCGPin* InPin, FName& OutExtraIcon, FText& OutTooltip, bool bIsOutPin = false) const;

protected:
	static TArray<PCGEx::FPinInfos> InPinInfos;
	static TArray<PCGEx::FPinInfos> OutPinInfos;
	static TMap<FName, int32> InPinInfosMap;
	static TMap<FName, int32> OutPinInfosMap;

	static bool bGeneratedPinMap;

	void GeneratePinInfos();
};
