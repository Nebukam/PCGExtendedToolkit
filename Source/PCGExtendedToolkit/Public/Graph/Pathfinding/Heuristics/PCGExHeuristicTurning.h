// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExHeuristicsFactoryProvider.h"
#include "UObject/Object.h"
#include "PCGExHeuristicOperation.h"
#include "Graph/PCGExCluster.h"
#include "PCGExHeuristicTurning.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExHeuristicConfigTurning : public FPCGExHeuristicConfigBase
{
	GENERATED_BODY()

	FPCGExHeuristicConfigTurning() :
		FPCGExHeuristicConfigBase()
	{
	}
};

/**
 * 
 */
UCLASS(DisplayName = "Turning")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicTurning : public UPCGExHeuristicOperation
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
		const PCGExCluster::FNode& Goal,
		const TArray<uint64>* TravelStack) const override
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
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicsFactoryTurning : public UPCGExHeuristicsFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExHeuristicConfigTurning Config;

	virtual UPCGExHeuristicOperation* CreateOperation() const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicsTurningProviderSettings : public UPCGExHeuristicsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		HeuristicsTurning, "Heuristics : Turning", "Heuristics based on turning/steering from last visited node.\n NOTE: Very expensive!",
		FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExHeuristicConfigTurning Config;


#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
