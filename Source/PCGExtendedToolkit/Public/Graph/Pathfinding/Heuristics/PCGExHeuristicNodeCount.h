// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Graph/PCGExCluster.h"
#include "PCGExHeuristicDistance.h"

#include "PCGExHeuristicNodeCount.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExHeuristicDescriptorLeastNodes : public FPCGExHeuristicDescriptorBase
{
	GENERATED_BODY()

	FPCGExHeuristicDescriptorLeastNodes() :
		FPCGExHeuristicDescriptorBase()
	{
	}
};

/**
 * 
 */
UCLASS(DisplayName = "Least Nodes")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicNodeCount : public UPCGExHeuristicDistance
{
	GENERATED_BODY()

public:
	virtual double GetEdgeScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& To,
		const PCGExGraph::FIndexedEdge& Edge,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override;
};

////

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGHeuristicsFactoryLeastNodes : public UPCGHeuristicsFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExHeuristicDescriptorLeastNodes Descriptor;

	virtual UPCGExHeuristicOperation* CreateOperation() const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicsLeastNodesProviderSettings : public UPCGExHeuristicsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		HeuristicsLeastNodes, "Heuristics : Least Nodes", "Heuristics based on node count.",
		FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	/** Filter Descriptor.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExHeuristicDescriptorLeastNodes Descriptor;

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
