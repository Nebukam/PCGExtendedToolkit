// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExCluster.h"
#include "UObject/Object.h"
#include "PCGExHeuristicOperation.h"
#include "PCGExHeuristicsFactoryProvider.h"
#include "PCGExHeuristicDistance.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExHeuristicDescriptorShortestDistance : public FPCGExHeuristicDescriptorBase
{
	GENERATED_BODY()

	FPCGExHeuristicDescriptorShortestDistance() :
		FPCGExHeuristicDescriptorBase()
	{
	}
};

/**
 * 
 */
UCLASS(DisplayName = "Shortest Distance")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicDistance : public UPCGExHeuristicOperation
{
	GENERATED_BODY()

public:
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

protected:
	double MaxDistSquared = 0;
};

////

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGHeuristicsFactoryShortestDistance : public UPCGHeuristicsFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExHeuristicDescriptorShortestDistance Descriptor;

	virtual UPCGExHeuristicOperation* CreateOperation() const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicsShortestDistanceProviderSettings : public UPCGExHeuristicsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		HeuristicsDistance, "Heuristics : Shortest Distance", "Heuristics based on distance.",
		FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	/** Filter Descriptor.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExHeuristicDescriptorShortestDistance Descriptor;

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
