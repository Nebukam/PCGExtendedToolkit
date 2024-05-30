// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"

#include "Data/PCGExGraphDefinition.h"

#include "PCGExHeuristicsFactoryProvider.generated.h"

#define PCGEX_FORWARD_HEURISTIC_FACTORY \
	NewFactory->WeightFactor = Descriptor.WeightFactor; \
	NewFactory->Descriptor = Descriptor;

#define PCGEX_FORWARD_HEURISTIC_DESCRIPTOR \
	NewOperation->WeightFactor = Descriptor.WeightFactor; \
	NewOperation->bInvert = Descriptor.bInvert; \
	NewOperation->ScoreCurve = Descriptor.ScoreCurve; \
	NewOperation->bUseLocalWeightMultiplier = Descriptor.bUseLocalWeightMultiplier; \
	NewOperation->LocalWeightMultiplierSource = Descriptor.LocalWeightMultiplierSource; \
	NewOperation->WeightMultiplierAttribute = Descriptor.WeightMultiplierAttribute;

class UPCGExHeuristicOperation;

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExHeuristicDescriptorBase
{
	GENERATED_BODY()

	FPCGExHeuristicDescriptorBase():
		ScoreCurve(PCGEx::WeightDistributionLinear)
	{
	}

	/** The weight factor for this heuristic.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	double WeightFactor = 1;

	/** Invert the heuristics so it looks away from the target instead of towards it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	bool bInvert = false;

	/** Curve the value will be remapped over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	TSoftObjectPtr<UCurveFloat> ScoreCurve;
	TObjectPtr<UCurveFloat> ScoreCurveObj;
	
	/** Use a local attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Local Weight", meta=(PCG_Overridable))
	bool bUseLocalWeightMultiplier = false;

	/** Local multiplier attribute source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Local Weight", meta=(PCG_Overridable, EditCondition="bUseLocalWeightMultiplier", EditConditionHides))
	EPCGExGraphValueSource LocalWeightMultiplierSource = EPCGExGraphValueSource::Point;
	
	/** Attribute to read multiplier value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Local Weight", meta=(PCG_Overridable, EditCondition="bUseLocalWeightMultiplier", EditConditionHides))
	FPCGAttributePropertyInputSelector WeightMultiplierAttribute;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGHeuristicsFactoryBase : public UPCGExParamFactoryBase
{
	GENERATED_BODY()

public:
	virtual UPCGExHeuristicOperation* CreateOperation() const;
	double WeightFactor = 1;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicsFactoryProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(NodeFilter, "Heuristics Definition", "Creates a single heuristic computational node, to be used with pathfinding nodes.")
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorHeuristics; }
#endif
	//~End UPCGSettings

	virtual FName GetMainOutputLabel() const override;
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;
};
