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

	/** How many previous edges should be averaged to compute the inertia. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=1))
	int32 Samples = 1;

	/** If enabled, use fallback score if there is less samples than the specified number. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=1))
	bool bIgnoreIfNotEnoughSamples = true;

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
	int32 MaxSamples = 1;
	bool bIgnoreIfNotEnoughSamples = true;

	virtual void PrepareForCluster(const TSharedPtr<const PCGExCluster::FCluster>& InCluster) override;

	FORCEINLINE virtual double GetGlobalScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override
	{
		return GetScoreInternal(GlobalInertiaScore);
	}

	FORCEINLINE virtual double GetEdgeScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& To,
		const PCGExGraph::FEdge& Edge,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal,
		const TSharedPtr<PCGEx::FHashLookup> TravelStack) const override
	{
		if (TravelStack)
		{
			int32 PathNodeIndex = PCGEx::NH64A(TravelStack->Get(From.NodeIndex));
			int32 PathEdgeIndex = -1;

			if (PathNodeIndex != -1)
			{
				FVector Avg = Cluster->GetDir(PathNodeIndex, From.NodeIndex);
				int32 Sampled = 1;
				while (PathNodeIndex != -1 && Sampled < MaxSamples)
				{
					const int32 CurrentIndex = PathNodeIndex;
					PCGEx::NH64(TravelStack->Get(CurrentIndex), PathNodeIndex, PathEdgeIndex);
					if (PathNodeIndex != -1)
					{
						Avg += Cluster->GetDir(PathNodeIndex, CurrentIndex);
						Sampled++;
					}
				}

				if (!bIgnoreIfNotEnoughSamples || Sampled == MaxSamples)
				{
					const double Dot = FVector::DotProduct(
						(Avg / Sampled).GetSafeNormal(),
						Cluster->GetDir(From.NodeIndex, To.NodeIndex));

					return GetScoreInternal(PCGExMath::Remap(Dot, -1, 1, OutMin, OutMax)) * ReferenceWeight;
				}
			}
		}

		return GetScoreInternal(FallbackInertiaScore);
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
