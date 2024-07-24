// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExHeuristicOperation.h"
#include "PCGExHeuristicsFactoryProvider.h"
#include "PCGExPointsProcessor.h"


#include "Graph/PCGExCluster.h"

#include "PCGExHeuristicAttribute.generated.h"

class UPCGExHeuristicOperation;

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExHeuristicAttributeConfig : public FPCGExHeuristicConfigBase
{
	GENERATED_BODY()

	FPCGExHeuristicAttributeConfig() :
		FPCGExHeuristicConfigBase()
	{
	}

	/** Read the data from either vertices or edges */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExGraphValueSource Source = EPCGExGraphValueSource::Vtx;

	/** Attribute to read modifier value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector Attribute;
};

/**
 * 
 */
UCLASS(DisplayName = "Direction")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicAttribute : public UPCGExHeuristicOperation
{
	GENERATED_BODY()

public:
	virtual void PrepareForCluster(const PCGExCluster::FCluster* InCluster) override;

	FORCEINLINE virtual double GetGlobalScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override
	{
		return 0;
	}

	FORCEINLINE virtual double GetEdgeScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& To,
		const PCGExGraph::FIndexedEdge& Edge,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal) const override
	{
		return CachedScores[Source == EPCGExGraphValueSource::Edge ? Edge.PointIndex : To.NodeIndex];
	}

	virtual void Cleanup() override;

	EPCGExGraphValueSource Source = EPCGExGraphValueSource::Vtx;
	FPCGAttributePropertyInputSelector Attribute;

protected:
	PCGExData::FPointIO* LastPoints = nullptr;
	TArray<double> CachedScores;
};


UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGHeuristicsFactoryAttribute : public UPCGExHeuristicsFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExHeuristicAttributeConfig Config;

	virtual UPCGExHeuristicOperation* CreateOperation() const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExCreateHeuristicAttributeSettings : public UPCGExHeuristicsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		HeuristicsAttribute, "Heuristics : Attribute", "Read a vtx or edge attribute as an heuristic value.",
		FName(GetDisplayName()))
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorHeuristicsAtt; }

#endif
	//~End UPCGSettings

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

	/** Modifier properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExHeuristicAttributeConfig Config;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
