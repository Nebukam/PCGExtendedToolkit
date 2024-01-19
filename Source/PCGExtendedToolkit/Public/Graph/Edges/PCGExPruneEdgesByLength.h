// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExPruneEdgesByLength.generated.h"

UENUM(BlueprintType)
enum class EPCGExEdgeThresholdMode : uint8
{
	Relative UMETA(DisplayName = "Relative", ToolTip="Regular pathfinding"),
	Absolute UMETA(DisplayName = "Absolute", ToolTip="Cell-based pathfinding"),
};

UENUM(BlueprintType)
enum class EPCGExEdgePruningThresholdMean : uint8
{
	Average UMETA(DisplayName = "Average", ToolTip="Average lengths"),
	Median UMETA(DisplayName = "Median", ToolTip="Median length"),
	Static UMETA(DisplayName = "Static", ToolTip="Static threshold"),
};

/**
 * A Base node to process a set of point using GraphParams.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExPruneEdgesByLengthSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PruneEdgesByLength, "Edges : Prune by Length", "Prune edges by length safely based on edges metrics. For more advanced/involved operations, prune edges yourself and use Sanitize Clusters.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorEdge; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExEdgesProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExEdgesProcessorSettings interface

	/** Removes roaming points from the output, and keeps only points that are part of an cluster. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bPruneIsolatedPoints = true;

	/** Value mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExEdgeThresholdMode Mode = EPCGExEdgeThresholdMode::Relative;

	/** Threshold reference value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExEdgePruningThresholdMean Mean = EPCGExEdgePruningThresholdMean::Average;

	/** Prune edges if their length is below a specific threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bPruneBelowThreshold = false;

	/** Minimum length threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bPruneBelowThreshold", ClampMin=0))
	double PruneBelow = 0;

	/** Prune edges if their length is below a specific threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bPruneAboveThreshold = false;

	/** Minimum length threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bPruneBelowThreshold", ClampMin=0))
	double PruneAbove = 0.9;

	/** Don't output Clusters if they have less points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sanitization", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveSmallClusters = false;

	/** Minimum points threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sanitization", meta = (PCG_Overridable, EditCondition="bRemoveSmallClusters", ClampMin=2))
	int32 MinClusterSize = 3;

	/** Don't output Clusters if they have more points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sanitization", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveBigClusters = false;

	/** Maximum points threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sanitization", meta = (PCG_Overridable, EditCondition="bRemoveBigClusters", ClampMin=2))
	int32 MaxClusterSize = 500;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPruneEdgesByLengthContext : public FPCGExEdgesProcessorContext
{
	friend class UPCGExPruneEdgesByLengthSettings;
	friend class FPCGExPruneEdgesByLengthElement;

	virtual ~FPCGExPruneEdgesByLengthContext() override;

	int32 MinClusterSize;
	int32 MaxClusterSize;

	PCGEx::TFAttributeReader<int32>* StartIndexReader = nullptr;
	PCGEx::TFAttributeReader<int32>* EndIndexReader = nullptr;

	double MinEdgeLength;
	double MaxEdgeLength;
	
	TArray<PCGExGraph::FIndexedEdge> Edges;
	TArray<double> EdgeLength;

	PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPruneEdgesByLengthElement : public FPCGExEdgesProcessorElement
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
