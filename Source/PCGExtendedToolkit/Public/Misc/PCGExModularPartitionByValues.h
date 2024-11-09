// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFilter.h"
#include "PCGExPartitionByValues.h"


#include "PCGExModularPartitionByValues.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPartitionRule : public UPCGExParamFactoryBase
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::RulePartition; }
	FPCGExPartitonRuleConfig Config;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPartitionRuleProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		PartitionRuleFactory, "Partition Rule", "Creates an single partition rule to be used with the Partition by Values node.",
		FName(GetDisplayName()))
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMisc; }
#endif
	//~End UPCGSettings

	//~Begin UPCGExFactoryProviderSettings
	virtual FName GetMainOutputLabel() const override { return FName("PartitionRule"); }
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
	//~End UPCGExFactoryProviderSettings

	/** Rule Config */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExPartitonRuleConfig Config;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExModularPartitionByValuesSettings : public UPCGExPartitionByValuesBaseSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ModularPartitionByValues, "Partition by Values", "Outputs separate buckets of points based on an attribute' value. Each bucket is named after a unique attribute value. Note that it is recommended to use a Merge before.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscAdd; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

public:
	virtual bool GetPartitionRules(FPCGExContext* InContext, TArray<FPCGExPartitonRuleConfig>& OutRules) const override;
};
