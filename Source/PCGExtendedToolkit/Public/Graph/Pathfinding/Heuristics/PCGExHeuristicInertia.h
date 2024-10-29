// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExHeuristicsFactoryProvider.h"
#include "UObject/Object.h"
#include "PCGExHeuristicOperation.h"
#include "Graph/PCGExCluster.h"
#include "PCGExHeuristicInertia.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExHeuristicConfigInertia : public FPCGExHeuristicConfigBase
{
	GENERATED_BODY()

	FPCGExHeuristicConfigInertia() :
		FPCGExHeuristicConfigBase()
	{
	}

	/** Value used for global score. Primarily used by A* Star to do initial sorting. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fallbacks", meta=(PCG_Overridable, DisplayPriority=-1, ClampMin=0, ClampMax=1))
	double GlobalInertiaScore = 0;

	/** Fallback heuristic score for when no inertia value can be computed (no previous node). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fallbacks", meta=(PCG_Overridable, DisplayPriority=-1, ClampMin=0, ClampMax=1))
	double FallbackInertiaScore = 0;
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Inertia")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExHeuristicInertia : public UPCGExHeuristicOperation
{
	GENERATED_BODY()

public:
	double GlobalInertiaScore = 0;
	double FallbackInertiaScore = 0;

	virtual void PrepareForCluster(const PCGExCluster::FCluster* InCluster) override;

	FORCEINLINE virtual double GetGlobalScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override
	{
		return FMath::Max(0, ScoreCurveObj->GetFloatValue(GlobalInertiaScore)) * ReferenceWeight;
	}

	FORCEINLINE virtual double GetEdgeScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& To,
		const PCGExGraph::FIndexedEdge& Edge,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal,
		const TArray<uint64>* TravelStack) const override
	{
		if (TravelStack)
		{
			if (const int32 PreviousNodeIndex = PCGEx::NH64A(*(TravelStack->GetData() + From.NodeIndex));
				PreviousNodeIndex != -1)
			{
				const double Dot = FVector::DotProduct(
					Cluster->GetDir(PreviousNodeIndex, From.NodeIndex),
					Cluster->GetDir(From.NodeIndex, To.NodeIndex));

				return FMath::Max(0, ScoreCurveObj->GetFloatValue(PCGExMath::Remap(Dot, -1, 1, OutMin, OutMax))) * ReferenceWeight;
			}
		}

		return FMath::Max(0, ScoreCurveObj->GetFloatValue(FallbackInertiaScore)) * ReferenceWeight;
	}

protected:
	double OutMin = 0;
	double OutMax = 1;
};

////

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExHeuristicsFactoryInertia : public UPCGExHeuristicsFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExHeuristicConfigInertia Config;

	virtual UPCGExHeuristicOperation* CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExHeuristicsInertiaProviderSettings : public UPCGExHeuristicsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		HeuristicsInertia, "Heuristics : Inertia", "Heuristics based on direction inertia from last visited node. NOTE: Can be quite expensive.",
		FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExHeuristicConfigInertia Config;


#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
