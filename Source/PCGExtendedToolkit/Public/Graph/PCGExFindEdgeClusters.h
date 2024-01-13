// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "IPCGExDebug.h"

#include "PCGExCustomGraphProcessor.h"
#include "Data/PCGExData.h"

#include "PCGExFindEdgeClusters.generated.h"

namespace PCGExGraph
{
	class FGraph;
	struct FEdgeCrossingsHandler;
}

UENUM(BlueprintType)
enum class EPCGExRoamingResolveMethod : uint8
{
	Overlap UMETA(DisplayName = "Overlap", ToolTip="Roaming nodes with unidirectional connections will create their own overlapping Clusters."),
	Merge UMETA(DisplayName = "Merge", ToolTip="Roaming Clusters will be merged into existing ones; thus creating less Clusters yet not canon ones."),
	Cutoff UMETA(DisplayName = "Cutoff", ToolTip="Roaming Clusters discovery will be cut off where they would otherwise overlap."),
};

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExFindEdgeClustersSettings : public UPCGExCustomGraphProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindEdgeClusters, "Custom Graph : Find Edge Clusters", "Create partitions from interconnected points. Each Cluster is the result of all input graphs combined.");
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual FName GetMainOutputLabel() const override;
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Edge types to crawl to create a Cluster */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExEdgeType"))
	uint8 CrawlEdgeTypes = static_cast<uint8>(EPCGExEdgeType::Complete);

	/** Removes roaming points from the output, and keeps only points that are part of an cluster. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bPruneIsolatedPoints = true;

	/** Don't output Clusters if they have less points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveSmallClusters = true;

	/** Minimum points threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bRemoveSmallClusters", ClampMin=2))
	int32 MinClusterSize = 3;

	/** Don't output Clusters if they have more points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveBigClusters = false;

	/** Maximum points threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bRemoveBigClusters", ClampMin=2))
	int32 MaxClusterSize = 500;

	/** If two edges are close enough, create a "crossing" point. !!! VERY EXPENSIVE !!! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bFindCrossings = false;

	/** Distance at which segments are considered crossing. !!! VERY EXPENSIVE !!! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bFindCrossings", ClampMin=0.001))
	double CrossingTolerance = 10;

	/** Edges will inherit point attributes -- NOT IMPLEMENTED*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bInheritAttributes = false;

	/** Name of the attribute to ouput the unique Cluster identifier to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName ClusterIDAttributeName = "ClusterID";

	/** Name of the attribute to output the Cluster size to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName ClusterSizeAttributeName = "ClusterSize";

	/** Cleanup graph socket data from output points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bDeleteCustomGraphData = true;

private:
	friend class FPCGExFindEdgeClustersElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFindEdgeClustersContext : public FPCGExCustomGraphProcessorContext
{
	friend class FPCGExFindEdgeClustersElement;

	virtual ~FPCGExFindEdgeClustersContext() override;

	EPCGExEdgeType CrawlEdgeTypes;
	bool bInheritAttributes;

	int32 MinClusterSize;
	int32 MaxClusterSize;

	FName ClusterIDAttributeName;
	FName ClusterSizeAttributeName;
	FName PointUIDAttributeName;

	PCGExGraph::FGraphBuilder* NetworkBuilder = nullptr;
};


class PCGEXTENDEDTOOLKIT_API FPCGExFindEdgeClustersElement : public FPCGExCustomGraphProcessorElement
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
