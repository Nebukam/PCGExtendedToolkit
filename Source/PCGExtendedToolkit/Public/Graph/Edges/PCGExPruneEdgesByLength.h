// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExPruneEdgesByLength.generated.h"

namespace PCGExCluster
{
	struct FCluster;
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPruneEdgesByLengthSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PruneEdgesByLength, "Cluster : Prune edges by Length", "Prune edges by length safely based on edges metrics. For more advanced/involved operations, prune edges yourself and use Sanitize Clusters.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorEdge; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExEdgesProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExEdgesProcessorSettings interface

	/** Measure mode. If using relative, threshold values should be kept between 0-1, while absolute use the world-space length of the edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExMeanMeasure Measure = EPCGExMeanMeasure::Relative;

	/** Which mean value is used to check whether an edge is above or below. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExMeanMethod MeanMethod = EPCGExMeanMethod::Average;

	/** Minimum length threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditConditionHides, EditCondition="MeanMethod==EPCGExMeanMethod::Fixed", ClampMin=0))
	double MeanValue = 0;

	/** Used to estimate the mode value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditConditionHides, EditCondition="MeanMethod==EPCGExMeanMethod::ModeMin || MeanMethod==EPCGExMeanMethod::ModeMax", ClampMin=0))
	double ModeTolerance = 5;

	/** Prune edges if their length is below a specific threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bPruneBelowMean = false;

	/** Minimum length threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bPruneBelowMean"))
	double PruneBelow = 0.2;

	/** Prune edges if their length is above a specific threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bPruneAboveMean = false;

	/** Maximum length threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bPruneAboveMean"))
	double PruneAbove = 0.2;

	/** Write Mean value used to an attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteMean = false;

	/** Attribute to write the Mean value into */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bWriteMean"))
	FName MeanAttributeName = "Mean";

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPruneEdgesByLengthContext final : public FPCGExEdgesProcessorContext
{
	friend class UPCGExPruneEdgesByLengthSettings;
	friend class FPCGExPruneEdgesByLengthElement;

	virtual ~FPCGExPruneEdgesByLengthContext() override;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPruneEdgesByLengthElement final : public FPCGExEdgesProcessorElement
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

namespace PCGExPruneEdges
{
	class FProcessor final : public PCGExClusterMT::FClusterProcessor
	{
		double ReferenceValue = 0;
		double ReferenceMin = 0;
		double ReferenceMax = 0;

		TArray<PCGExGraph::FIndexedEdge> IndexedEdges;
		TArray<double> EdgeLengths;

	public:
		FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
			FClusterProcessor(InVtx, InEdges)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}
