// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExFindEdgesIntersections.generated.h"

/**
 * A Base node to process a set of point using GraphParams.
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph") //TODO : Not abstract, just hides while WIP
class PCGEXTENDEDTOOLKIT_API UPCGExFindEdgesIntersectionsSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindEdgesIntersections, "Edges : Find Intersections", "Find and writes edge intersections");
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

	/** Attempts to merge all edge clusters associated with a vtx input before proceeding to finding crossings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bMergeBoundClusters = true;

	/** Distance at which segments are considered crossing. !!! VERY EXPENSIVE !!!*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bFindCrossings", ClampMin=0.001))
	double CrossingTolerance = 10;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Graph Output Settings"))
	FPCGExGraphBuilderSettings GraphBuilderSettings;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFindEdgesIntersectionsContext : public FPCGExEdgesProcessorContext
{
	friend class UPCGExFindEdgesIntersectionsSettings;
	friend class FPCGExFindEdgesIntersectionsElement;

	virtual ~FPCGExFindEdgesIntersectionsContext() override;

	double CrossingTolerance;

	FPCGExGraphBuilderSettings GraphBuilderSettings;
	PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExFindEdgesIntersectionsElement : public FPCGExEdgesProcessorElement
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

class PCGEXTENDEDTOOLKIT_API FPCGExFindIntersectionsTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExFindIntersectionsTask(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		PCGExGraph::FGraph* InGraph) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		Graph(InGraph)
	{
	}

	PCGExGraph::FGraph* Graph = nullptr;

	virtual bool ExecuteTask() override;
};
