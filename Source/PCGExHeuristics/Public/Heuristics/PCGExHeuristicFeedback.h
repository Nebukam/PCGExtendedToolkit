// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExHeuristicOperation.h"
#include "UObject/Object.h"
#include "Core/PCGExHeuristicsFactoryProvider.h"

#include "PCGExHeuristicFeedback.generated.h"

USTRUCT(BlueprintType)
struct FPCGExHeuristicConfigFeedback : public FPCGExHeuristicConfigBase
{
	GENERATED_BODY()

	FPCGExHeuristicConfigFeedback()
		: FPCGExHeuristicConfigBase()
	{
	}

	/** If enabled, weight doesn't scale with overlap; the base score is either 0 or 1. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0, ClampMax=1))
	bool bBinary = false;

	/** Weight to add to points that are already part of the plotted path. This is used to sample the weight curve.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bBinary", ClampMin=0, ClampMax=1))
	double VisitedPointsWeightFactor = 1;

	/** Weight to add to edges that are already part of the plotted path. This is used to sample the weight curve.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bBinary", ClampMin=0, ClampMax=1))
	double VisitedEdgesWeightFactor = 1;

	/** Global feedback weight persist between path query in a single pathfinding node.
	 * IMPORTANT NOTE: This break parallelism, and may be slower.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bGlobalFeedback = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAffectAllConnectedEdges = true;
};

/**
 * 
 */
class FPCGExHeuristicFeedback : public FPCGExHeuristicOperation
{
	mutable FRWLock FeedbackLock;
	TMap<int32, uint32> NodeFeedbackNum;
	TMap<int32, uint32> EdgeFeedbackNum;

public:
	double NodeScale = 1;
	double EdgeScale = 1;
	bool bBleed = true;
	bool bBinary = false;

	virtual double GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal) const override;

	virtual double GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const TSharedPtr<PCGEx::FHashLookup> TravelStack) const override;

	void FeedbackPointScore(const PCGExClusters::FNode& Node);

	void FeedbackScore(const PCGExClusters::FNode& Node, const PCGExGraphs::FEdge& Edge);
};

////

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExHeuristicsFactoryFeedback : public UPCGExHeuristicsFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExHeuristicConfigFeedback Config;

	virtual bool IsGlobal() const { return Config.bGlobalFeedback; }

	virtual TSharedPtr<FPCGExHeuristicOperation> CreateOperation(FPCGExContext* InContext) const override;
	PCGEX_HEURISTIC_FACTORY_BOILERPLATE
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params", meta=(PCGExNodeLibraryDoc="pathfinding/heuristics/hx-feedback"))
class UPCGExHeuristicFeedbackProviderSettings : public UPCGExHeuristicsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(HeuristicsFeedback, "Heuristics : Feedback", "Heuristics based on visited score feedback.", FName(GetDisplayName()))
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(HeuristicsFeedback); }
#endif
	//~End UPCGSettings

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExHeuristicConfigFeedback Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
