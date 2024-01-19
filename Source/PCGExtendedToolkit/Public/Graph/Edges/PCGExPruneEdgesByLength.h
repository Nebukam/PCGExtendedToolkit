// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExPruneEdgesByLength.generated.h"

UENUM(BlueprintType)
enum class EPCGExEdgeLengthMeasure : uint8
{
	Relative UMETA(DisplayName = "Relative", ToolTip="Edge length will be normalized between 0..1"),
	Absolute UMETA(DisplayName = "Absolute", ToolTip="Raw edge length will be used."),
};

UENUM(BlueprintType)
enum class EPCGExEdgeMeanMethod : uint8
{
	Average UMETA(DisplayName = "Average", ToolTip="Average length"),
	Median UMETA(DisplayName = "Median", ToolTip="Median length"),
	ModeMin UMETA(DisplayName = "Mode (Shortest)", ToolTip="Mode length (~= longest most common length)"),
	ModeMax UMETA(DisplayName = "Mode (Longest)", ToolTip="Mode length (~= shortest most common length)"),
	Central UMETA(DisplayName = "Central", ToolTip="Central uses the middle value between Min/Max edge lengths."),
	Fixed UMETA(DisplayName = "Fixed", ToolTip="Fixed threshold"),
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

	/** Measure mode. If using relative, threshold values should be kept between 0-1, while absolute use the world-space length of the edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExEdgeLengthMeasure Measure = EPCGExEdgeLengthMeasure::Relative;

	/** Which mean value is used to check whether an edge is above or below. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExEdgeMeanMethod MeanMethod = EPCGExEdgeMeanMethod::Average;

	/** Minimum length threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditConditionHides, EditCondition="Mean==EPCGExEdgePruningThresholdMean::Fixed", ClampMin=0))
	double MeanValue = 0;

	/** Used to estimate the mode value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditConditionHides, EditCondition="Mean==EPCGExEdgePruningThresholdMean::Mode", ClampMin=0))
	double ModeTolerance = 0;

	/** Prune edges if their length is below a specific threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bPruneBelowMean = false;

	/** Minimum length threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bPruneBelowMean", ClampMin=0.001))
	double PruneBelow = 0.2;

	/** Prune edges if their length is below a specific threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bPruneAboveMean = false;

	/** Minimum length threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bPruneAboveMean", ClampMin=0.001))
	double PruneAbove = 0.2;

	/** Write Mean value used to an attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteMean = false;

	/** Attribute to write the Mean value into */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bWriteMean"))
	FName MeanAttributeName = "Mean";

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

	double ReferenceValue;
	double ReferenceMin;
	double ReferenceMax;

	TArray<PCGExGraph::FIndexedEdge> IndexedEdges;

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
