// Copyright 2024 Timothé Lapetite and contributors
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

	/** Weight to add to points that are already part of the plotted path. This is used to sample the weight curve.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0, ClampMax=1))
	double VisitedPointsWeightFactor = 1;

	/** Weight to add to edges that are already part of the plotted path. This is used to sample the weight curve.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0, ClampMax=1))
	double VisitedEdgesWeightFactor = 1;

	/** Global feedback weight persist between path query in a single pathfinding node.  IMPORTANT NOTE: This break parallelism, and may be slower.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bGlobalFeedback = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAffectAllConnectedEdges = true;
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Feedback")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExHeuristicFeedback : public UPCGExHeuristicOperation
{
	GENERATED_BODY()

	mutable FRWLock FeedbackLock;
	TMap<int32, uint32> NodeFeedbackNum;
	TMap<int32, uint32> EdgeFeedbackNum;

public:
	double NodeScale = 1;
	double EdgeScale = 1;
	bool bBleed = true;

	FORCEINLINE virtual double GetGlobalScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override
	{
		FReadScopeLock ReadScopeLock(FeedbackLock);

		const uint32* N = NodeFeedbackNum.Find(From.Index);
		return N ? GetScoreInternal(NodeScale) * *N : GetScoreInternal(0);
	}

	FORCEINLINE virtual double GetEdgeScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& To,
		const PCGExGraph::FEdge& Edge,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal,
		const TSharedPtr<PCGEx::FHashLookup> TravelStack) const override
	{
		FReadScopeLock ReadScopeLock(FeedbackLock);

		const uint32* N = NodeFeedbackNum.Find(To.Index);
		const uint32* E = EdgeFeedbackNum.Find(Edge.Index);

		const double NW = N ? GetScoreInternal(NodeScale) * *N : GetScoreInternal(0);
		const double EW = E ? GetScoreInternal(EdgeScale) * *E : GetScoreInternal(0);

		return (NW + EW);
	}

	FORCEINLINE void FeedbackPointScore(const PCGExCluster::FNode& Node)
	{
		FWriteScopeLock WriteScopeLock(FeedbackLock);

		uint32& N = NodeFeedbackNum.FindOrAdd(Node.Index, 0);
		N++;

		if (bBleed)
		{
			for (const PCGExGraph::FLink Lk : Node.Links)
			{
				uint32& E = EdgeFeedbackNum.FindOrAdd(Lk.Edge, 0);
				E++;
			}
		}
	}

	FORCEINLINE void FeedbackScore(const PCGExCluster::FNode& Node, const PCGExGraph::FEdge& Edge)
	{
		FWriteScopeLock WriteScopeLock(FeedbackLock);

		uint32& N = NodeFeedbackNum.FindOrAdd(Node.Index, 0);
		N++;

		if (bBleed)
		{
			for (const PCGExGraph::FLink Lk : Node.Links)
			{
				uint32& E = EdgeFeedbackNum.FindOrAdd(Lk.Edge, 0);
				E++;
			}
		}
		else
		{
			uint32& E = EdgeFeedbackNum.FindOrAdd(Edge.Index, 0);
			E++;
		}
	}

	virtual void Cleanup() override;
};

////

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExHeuristicsFactoryFeedback : public UPCGExHeuristicsFactoryData
{
	GENERATED_BODY()

public:
	virtual bool IsGlobal() const { return Config.bGlobalFeedback; }

	FPCGExHeuristicConfigFeedback Config;

	virtual UPCGExHeuristicOperation* CreateOperation(FPCGExContext* InContext) const override;
	PCGEX_HEURISTIC_FACTORY_BOILERPLATE
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
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorHeuristicsFeedback; }
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
