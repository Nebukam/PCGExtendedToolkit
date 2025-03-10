// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExHeuristicsFactoryProvider.h"
#include "UObject/Object.h"
#include "PCGExHeuristicOperation.h"


#include "Graph/PCGExCluster.h"
#include "PCGExHeuristicAzimuth.generated.h"

USTRUCT(BlueprintType)
struct FPCGExHeuristicConfigAzimuth : public FPCGExHeuristicConfigBase
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
class UPCGExHeuristicAzimuth : public UPCGExHeuristicOperation
{
	GENERATED_BODY()

public:
	virtual double GetGlobalScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override;

	virtual double GetEdgeScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& To,
		const PCGExGraph::FEdge& Edge,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal,
		const TSharedPtr<PCGEx::FHashLookup> TravelStack) const override;
};

////

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExHeuristicsFactoryAzimuth : public UPCGExHeuristicsFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExHeuristicConfigAzimuth Config;

	virtual UPCGExHeuristicOperation* CreateOperation(FPCGExContext* InContext) const override;
	PCGEX_HEURISTIC_FACTORY_BOILERPLATE
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class UPCGExHeuristicsAzimuthProviderSettings : public UPCGExHeuristicsFactoryProviderSettings
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

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExHeuristicConfigAzimuth Config;


#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
