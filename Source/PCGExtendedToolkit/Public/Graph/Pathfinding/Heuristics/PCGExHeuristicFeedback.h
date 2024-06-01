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

	/** Global feedback weight persist between path query in a single pathfinding node. \n IMPORTANT NOTE: This break parallelism, and may be slower.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bGlobalFeedback = false;
};

/**
 * 
 */
UCLASS(DisplayName = "Feedback")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicFeedback : public UPCGExHeuristicOperation
{
	GENERATED_BODY()

	TMap<int32, double> NodeExtraWeight;
	TMap<int32, double> EdgeExtraWeight;

	double MaxNodeWeight = 0;
	double MaxEdgeWeight = 0;

public:
	double NodeScale = 1;
	double EdgeScale = 1;

	virtual void PrepareForCluster(PCGExCluster::FCluster* InCluster) override;

	FORCEINLINE virtual double GetGlobalScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override;

	FORCEINLINE virtual double GetEdgeScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& To,
		const PCGExGraph::FIndexedEdge& Edge,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override;

	FORCEINLINE void FeedbackPointScore(const PCGExCluster::FNode& Node);
	FORCEINLINE void FeedbackScore(const PCGExCluster::FNode& Node, const PCGExGraph::FIndexedEdge& Edge);

	virtual void Cleanup() override;
};

////

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGHeuristicsFactoryFeedback : public UPCGHeuristicsFactoryBase
{
	GENERATED_BODY()

public:
	virtual bool IsGlobal() const { return Descriptor.bGlobalFeedback; }

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
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		HeuristicsFeedback, "Heuristics : Feedback", "Heuristics based on visited score feedback.",
		FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	/** Filter Descriptor.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExHeuristicDescriptorFeedback Descriptor;

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
