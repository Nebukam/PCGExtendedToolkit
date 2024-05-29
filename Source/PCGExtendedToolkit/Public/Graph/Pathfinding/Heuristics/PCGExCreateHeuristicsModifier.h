// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExHeuristicsFactoryProvider.h"
#include "PCGExPointsProcessor.h"

#include "Data/PCGExGraphDefinition.h"

#include "PCGExCreateHeuristicsModifier.generated.h"

class UPCGExHeuristicOperation;

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExHeuristicModifierDescriptor : public FPCGExHeuristicDescriptorBase
{
	GENERATED_BODY()

	FPCGExHeuristicModifierDescriptor() :
		FPCGExHeuristicDescriptorBase(),
		ScoreCurve(PCGEx::WeightDistributionLinear)
	{
	}

	/** Read the data from either vertices or edges */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExGraphValueSource Source = EPCGExGraphValueSource::Point;

	/** Fetch weight from attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bUseLocalWeightFactor = false;

	/** Attribute to fetch local weight from. This value will be scaled by the base weight. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseLocalWeight"))
	FPCGAttributePropertyInputSelector WeightFactorAttribute;

	/** Curve the value will be remapped over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UCurveFloat> ScoreCurve;
	TObjectPtr<UCurveFloat> ScoreCurveObj;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGHeuristicsModiferFactory : public UPCGExParamFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExHeuristicModifierDescriptor Descriptor;
	
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExCreateHeuristicsModifierSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(NodeFilter, "Heuristics : Modifier", "Creates a single heuristic modifier settings bloc.")
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorHeuristics; }

#endif
	//~End UPCGSettings

	virtual FName GetMainOutputLabel() const override;
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Modifier properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExHeuristicModifierDescriptor Descriptor;
};
