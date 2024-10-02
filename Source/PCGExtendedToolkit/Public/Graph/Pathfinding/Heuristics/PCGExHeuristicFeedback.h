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
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExHeuristicConfigFeedback : public FPCGExHeuristicConfigBase
{
	GENERATED_BODY()

	FPCGExHeuristicConfigFeedback() :
		FPCGExHeuristicConfigBase()
	{
	}

	/** Weight to add to points that are already part of the plotted path. This is a multplier of the Reference Weight.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double VisitedPointsWeightFactor = 1;

	/** Weight to add to edges that are already part of the plotted path. This is a multplier of the Reference Weight.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double VisitedEdgesWeightFactor = 1;

	/** Global feedback weight persist between path query in a single pathfinding node.  IMPORTANT NOTE: This break parallelism, and may be slower.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bGlobalFeedback = false;
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Feedback")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExHeuristicFeedback : public UPCGExHeuristicOperation
{
	GENERATED_BODY()

	TMap<int32, double> NodeExtraWeight;
	TMap<int32, double> EdgeExtraWeight;

	double MaxNodeWeight = 0;
	double MaxEdgeWeight = 0;

public:
	double NodeScale = 1;
	double EdgeScale = 1;

	virtual void PrepareForCluster(const PCGExCluster::FCluster* InCluster) override;

	FORCEINLINE virtual double GetGlobalScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override
	{
		return NodeExtraWeight[From.NodeIndex];
	}

	FORCEINLINE virtual double GetEdgeScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& To,
		const PCGExGraph::FIndexedEdge& Edge,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal,
		const TArray<uint64>* TravelStack) const override
	{
		const double* NodePtr = NodeExtraWeight.Find(To.NodeIndex);
		const double* EdgePtr = EdgeExtraWeight.Find(Edge.EdgeIndex);

		return ((NodePtr ? SampleCurve(*NodePtr / MaxNodeWeight) * ReferenceWeight : 0) + (EdgePtr ? SampleCurve(*EdgePtr / MaxEdgeWeight) * ReferenceWeight : 0));
	}

	FORCEINLINE void FeedbackPointScore(const PCGExCluster::FNode& Node)
	{
		double& NodeWeight = NodeExtraWeight.FindOrAdd(Node.PointIndex);
		MaxNodeWeight = FMath::Max(MaxNodeWeight, NodeWeight += ReferenceWeight * NodeScale);
	}

	FORCEINLINE void FeedbackScore(const PCGExCluster::FNode& Node, const PCGExGraph::FIndexedEdge& Edge)
	{
		double& NodeWeight = NodeExtraWeight.FindOrAdd(Node.PointIndex);
		double& EdgeWeight = NodeExtraWeight.FindOrAdd(Edge.EdgeIndex);
		MaxNodeWeight = FMath::Max(MaxNodeWeight, NodeWeight += ReferenceWeight * NodeScale);
		MaxEdgeWeight = FMath::Max(MaxEdgeWeight, EdgeWeight += ReferenceWeight * EdgeScale);
	}

	virtual void Cleanup() override;
};

////

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExHeuristicsFactoryFeedback : public UPCGExHeuristicsFactoryBase
{
	GENERATED_BODY()

public:
	virtual bool IsGlobal() const { return Config.bGlobalFeedback; }

	FPCGExHeuristicConfigFeedback Config;
	virtual UPCGExHeuristicOperation* CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExHeuristicFeedbackProviderSettings : public UPCGExHeuristicsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		HeuristicsFeedback, "Heuristics : Feedback", "Heuristics based on visited score feedback.",
		FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExHeuristicConfigFeedback Config;

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
