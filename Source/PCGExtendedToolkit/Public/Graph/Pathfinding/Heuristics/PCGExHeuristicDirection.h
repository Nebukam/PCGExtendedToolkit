// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExHeuristicsFactoryProvider.h"
#include "UObject/Object.h"
#include "PCGExHeuristicOperation.h"
#include "Graph/PCGExCluster.h"
#include "PCGExHeuristicDirection.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExHeuristicConfigDirection : public FPCGExHeuristicConfigBase
{
	GENERATED_BODY()

	FPCGExHeuristicConfigDirection() :
		FPCGExHeuristicConfigBase()
	{
	}
};

/**
 * 
 */
UCLASS(DisplayName = "Direction")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicDirection : public UPCGExHeuristicOperation
{
	GENERATED_BODY()

public:
	virtual void PrepareForCluster(const PCGExCluster::FCluster* InCluster) override;

	FORCEINLINE virtual double GetGlobalScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override
	{
		const FVector Dir = Cluster->GetDir(Seed, Goal);
		const double Dot = FVector::DotProduct(Dir, Cluster->GetDir(From, Goal)) * -1;
		return FMath::Max(0, ScoreCurveObj->GetFloatValue(PCGExMath::Remap(Dot, -1, 1, OutMin, OutMax))) * ReferenceWeight;
	}

	FORCEINLINE virtual double GetEdgeScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& To,
		const PCGExGraph::FIndexedEdge& Edge,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override
	{
		const double Dot = (FVector::DotProduct(Cluster->GetDir(From, To), Cluster->GetDir(From, Goal)) * -1);
		return FMath::Max(0, ScoreCurveObj->GetFloatValue(PCGExMath::Remap(Dot, -1, 1, OutMin, OutMax))) * ReferenceWeight;
	}

protected:
	double OutMin = 0;
	double OutMax = 1;
};

////

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGHeuristicsFactoryDirection : public UPCGExHeuristicsFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExHeuristicConfigDirection Config;

	virtual UPCGExHeuristicOperation* CreateOperation() const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicsDirectionProviderSettings : public UPCGExHeuristicsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		HeuristicsDirection, "Heuristics : Direction", "Heuristics based on direction.",
		FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExHeuristicConfigDirection Config;


#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
