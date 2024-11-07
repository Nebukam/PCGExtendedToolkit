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
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExHeuristicAttributeConfig : public FPCGExHeuristicConfigBase
{
	GENERATED_BODY()

	FPCGExHeuristicAttributeConfig() :
		FPCGExHeuristicConfigBase()
	{
	}

	/** Read the data from either vertices or edges */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExClusterComponentSource Source = EPCGExClusterComponentSource::Vtx;

	/** Attribute to read modifier value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector Attribute;
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Attribute")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExHeuristicAttribute : public UPCGExHeuristicOperation
{
	GENERATED_BODY()

public:
	virtual void PrepareForCluster(const TSharedPtr<const PCGExCluster::FCluster>& InCluster) override;

	FORCEINLINE virtual double GetEdgeScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& To,
		const PCGExGraph::FIndexedEdge& Edge,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal,
		const TSharedPtr<PCGEx::FHashLookup> TravelStack) const override
	{
		return CachedScores[Source == EPCGExClusterComponentSource::Edge ? Edge.PointIndex : To.NodeIndex];
	}

	virtual void Cleanup() override
	{
		CachedScores.Empty();
		LastPoints.Reset();
		Super::Cleanup();
	}

	EPCGExClusterComponentSource Source = EPCGExClusterComponentSource::Vtx;
	FPCGAttributePropertyInputSelector Attribute;

protected:
	TSharedPtr<PCGExData::FPointIO> LastPoints;
	TArray<double> CachedScores;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExHeuristicsFactoryAttribute : public UPCGExHeuristicsFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExHeuristicAttributeConfig Config;

	virtual UPCGExHeuristicOperation* CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExCreateHeuristicAttributeSettings : public UPCGExHeuristicsFactoryProviderSettings
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
