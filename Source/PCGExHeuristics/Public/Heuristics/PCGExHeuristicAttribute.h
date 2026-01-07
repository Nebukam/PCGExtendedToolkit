// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExHeuristicOperation.h"
#include "Core/PCGExHeuristicsFactoryProvider.h"
#include "PCGExHeuristicAttribute.generated.h"

class FPCGExHeuristicOperation;

UENUM()
enum class EPCGExAttributeHeuristicInputMode : uint8
{
	AutoCurve   = 0 UMETA(DisplayName = "Auto Curve", ToolTip="Automatically sample the curve using normalized value from existing min/max input."),
	ManualCurve = 1 UMETA(DisplayName = "Manual Curve", ToolTip="Sample the curve using normalized value from manual min/max values."),
	Raw         = 2 UMETA(DisplayName = "Raw", ToolTip="Use raw attribute as score. Use at your own risks!"),
};

USTRUCT(BlueprintType)
struct FPCGExHeuristicAttributeConfig : public FPCGExHeuristicConfigBase
{
	GENERATED_BODY()

	FPCGExHeuristicAttributeConfig()
		: FPCGExHeuristicConfigBase()
	{
	}

	/** Specify how to deal with the attribute value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayPriority=-2))
	EPCGExAttributeHeuristicInputMode Mode = EPCGExAttributeHeuristicInputMode::AutoCurve;

	/** Read the data from either vertices or edges */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExClusterElement Source = EPCGExClusterElement::Vtx;

	/** Attribute to read modifier value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector Attribute;

	/** If enabled, will use this value as input min remap reference instead of the one found on the attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode == EPCGExAttributeHeuristicInputMode::ManualCurve", EditConditionHides))
	double InMin = 0;

	/** If enabled, will use this value as input max remap reference instead of the one found on the attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode == EPCGExAttributeHeuristicInputMode::ManualCurve", EditConditionHides))
	double InMax = 1;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bUseCustomFallback = false;

	/** Default weight when no valid internal normalization can be made (e.g, all points have the same values so min == max). If left unset, will use min/max clamped between 0 & 1. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseCustomFallback", ClampMin=0, ClampMax=1))
	double FallbackValue = 1;
};

/**
 * 
 */
class FPCGExHeuristicAttribute : public FPCGExHeuristicOperation
{
public:
	virtual void PrepareForCluster(const TSharedPtr<const PCGExClusters::FCluster>& InCluster) override;

	virtual double GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const TSharedPtr<PCGEx::FHashLookup> TravelStack) const override;

	EPCGExClusterElement Source = EPCGExClusterElement::Vtx;
	FPCGAttributePropertyInputSelector Attribute;
	EPCGExAttributeHeuristicInputMode Mode = EPCGExAttributeHeuristicInputMode::AutoCurve;
	bool bUseCustomFallback = false;
	double FallbackValue = 1;

	double InMin = 0;
	double InMax = 100;

protected:
	TArray<double> CachedScores;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExHeuristicsFactoryAttribute : public UPCGExHeuristicsFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExHeuristicAttributeConfig Config;

	virtual TSharedPtr<FPCGExHeuristicOperation> CreateOperation(FPCGExContext* InContext) const override;
	PCGEX_HEURISTIC_FACTORY_BOILERPLATE

	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params", meta=(PCGExNodeLibraryDoc="pathfinding/heuristics/hx-attribute"))
class UPCGExCreateHeuristicAttributeSettings : public UPCGExHeuristicsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(HeuristicsAttribute, "Heuristics : Attribute", "Read a vtx or edge attribute as an heuristic value.", FName(GetDisplayName()))
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(HeuristicsAttribute); }

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UPCGSettings

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

	/** Modifier properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExHeuristicAttributeConfig Config;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
