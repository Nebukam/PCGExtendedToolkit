// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExHeuristicsFactoryProvider.h"
#include "UObject/Object.h"
#include "PCGExHeuristicOperation.h"
#include "Graph/PCGExCluster.h"

#include "PCGExHeuristicFeedback.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExHeuristicDescriptorFeedback : public FPCGExHeuristicDescriptorBase
{
	GENERATED_BODY()

	FPCGExHeuristicDescriptorFeedback() :
		FPCGExHeuristicDescriptorBase()
	{
	}
	
	/** Weight to add to points that are already part of the plotted path. This is a multplier of the Reference Weight.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double VisitedPointsWeightFactor = 1;

	/** Weight to add to edges that are already part of the plotted path. This is a multplier of the Reference Weight.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double VisitedEdgesWeightFactor = 1;

	/** Shares visited weight between pathfinding queries. Slow as it break parallelism. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bGlobalFeedback = true;
	
};

/**
 * 
 */
UCLASS(DisplayName = "Feedback")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicFeedback : public UPCGExHeuristicOperation
{
	GENERATED_BODY()

	TArray<double> NodeExtraWeight;
	TArray<double> EdgeExtraWeight;

public:
	double NodeScale = 1;
	double EdgeScale = 1;

	bool bGlobalFeedback = true;
	
	virtual void PrepareForData(PCGExCluster::FCluster* InCluster) override;

	virtual double GetGlobalScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override;

	virtual double GetEdgeScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& To,
		const PCGExGraph::FIndexedEdge& Edge,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override;

	void FeedbackPointScore(const PCGExCluster::FNode& Node);
	void FeedbackScore(const PCGExCluster::FNode& Node, const PCGExGraph::FIndexedEdge& Edge);

	virtual void Cleanup() override;
};

////

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGHeuristicsFactoryFeedback : public UPCGHeuristicsFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExHeuristicDescriptorFeedback Descriptor;

	virtual UPCGExHeuristicOperation* CreateOperation() const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicFeedbackProviderSettings : public UPCGExHeuristicsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(NodeFilter, "Heuristics : Feedback", "Heuristics based on visited score feedback.")
#endif
	//~End UPCGSettings

	/** Filter Descriptor.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExHeuristicDescriptorFeedback Descriptor;

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;
	
};
