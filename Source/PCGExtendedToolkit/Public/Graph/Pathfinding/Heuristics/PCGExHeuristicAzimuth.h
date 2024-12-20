// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExHeuristicsFactoryProvider.h"
#include "UObject/Object.h"
#include "PCGExHeuristicOperation.h"


#include "Graph/PCGExCluster.h"
#include "PCGExHeuristicAzimuth.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExHeuristicConfigAzimuth : public FPCGExHeuristicConfigBase
{
	GENERATED_BODY()

	FPCGExHeuristicConfigAzimuth() :
		FPCGExHeuristicConfigBase()
	{
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Azimuth")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExHeuristicAzimuth : public UPCGExHeuristicOperation
{
	GENERATED_BODY()

public:
	virtual void PrepareForCluster(const TSharedPtr<const PCGExCluster::FCluster>& InCluster) override;

	FORCEINLINE virtual double GetGlobalScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override
	{
		const FVector Dir = Cluster->GetDir(Seed, Goal);
		const double Dot = FVector::DotProduct(Dir, Cluster->GetDir(From, Goal)) * -1;
		return GetScoreInternal(PCGExMath::Remap(Dot, -1, 1, OutMin, OutMax));
	}

	FORCEINLINE virtual double GetEdgeScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& To,
		const PCGExGraph::FEdge& Edge,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal,
		const TSharedPtr<PCGEx::FHashLookup> TravelStack) const override
	{
		const double Dot = (FVector::DotProduct(Cluster->GetDir(From, To), Cluster->GetDir(From, Goal)) * -1);
		return GetScoreInternal(PCGExMath::Remap(Dot, -1, 1, OutMin, OutMax));
	}

protected:
	double OutMin = 0;
	double OutMax = 1;
};

////

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExHeuristicsFactoryAzimuth : public UPCGExHeuristicsFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExHeuristicConfigAzimuth Config;

	virtual UPCGExHeuristicOperation* CreateOperation(FPCGExContext* InContext) const override;
	PCGEX_HEURISTIC_FACTORY_BOILERPLATE
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExHeuristicsAzimuthProviderSettings : public UPCGExHeuristicsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		HeuristicsAzimuth, "Heuristics : Azimuth", "Heuristics based on direction toward final goal (north star).",
		FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExHeuristicConfigAzimuth Config;


#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
