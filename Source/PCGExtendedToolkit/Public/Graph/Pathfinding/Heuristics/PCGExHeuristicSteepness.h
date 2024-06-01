// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExHeuristicDistance.h"
#include "UObject/Object.h"
#include "PCGExHeuristicOperation.h"
#include "Graph/PCGExCluster.h"
#include "PCGExHeuristicSteepness.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExHeuristicDescriptorSteepness : public FPCGExHeuristicDescriptorBase
{
	GENERATED_BODY()

	FPCGExHeuristicDescriptorSteepness() :
		FPCGExHeuristicDescriptorBase()
	{
	}

	/** Vector pointing in the "up" direction. Mirrored. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FVector UpVector = FVector::UpVector;
};

/**
 * 
 */
UCLASS(DisplayName = "Steepness")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicSteepness : public UPCGExHeuristicDistance
{
	GENERATED_BODY()

public:
	/** Vector pointing in the "up" direction. Mirrored. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FVector UpVector = FVector::UpVector;

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
	FVector UpwardVector = FVector::UpVector;

	FORCEINLINE double GetDot(const FVector& From, const FVector& To) const;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGHeuristicsFactorySteepness : public UPCGHeuristicsFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExHeuristicDescriptorSteepness Descriptor;

	virtual UPCGExHeuristicOperation* CreateOperation() const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicsSteepnessProviderSettings : public UPCGExHeuristicsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		HeuristicsSteepness, "Heuristics : Steepness", "Heuristics based on steepness.",
		FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	/** Filter Descriptor.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExHeuristicDescriptorSteepness Descriptor;

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
