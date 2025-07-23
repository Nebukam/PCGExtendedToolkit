// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDetailsData.h"
#include "PCGExMatchRuleFactoryProvider.h"
#include "PCGExPointsProcessor.h"

#include "PCGExMatchAttrToAttrNum.generated.h"

USTRUCT(BlueprintType)
struct FPCGExMatchAttrToAttrNumConfig : public FPCGExMatchRuleConfigBase
{
	GENERATED_BODY()

	FPCGExMatchAttrToAttrNumConfig() :
		FPCGExMatchRuleConfigBase()
	{
	}

	/** The attribute to read from on the targets. Depending on where the match operate, this can be read on a target point or data domain. If only data domain is supported, will read first element value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName TargetAttributeName = FName("@Data.Target");

	/** The attribute to read on the candidates (the data that's not used as target). Only support @Data domain, and will only try to read from there. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName CandidateAttributeName = FName("@Data.Input");

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExComparison Comparison = EPCGExComparison::StrictlyEqual;

	/** Rounding mode for relative measures */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Comparison == EPCGExComparison::NearlyEqual || Comparison == EPCGExComparison::NearlyNotEqual", EditConditionHides))
	double Tolerance = DBL_COMPARE_TOLERANCE;
};

/**
 * 
 */
class FPCGExMatchAttrToAttrNum : public FPCGExMatchRuleOperation
{
public:
	FPCGExMatchAttrToAttrNumConfig Config;

	virtual bool PrepareForTargets(FPCGExContext* InContext, const TSharedPtr<TArray<PCGExData::FTaggedData>>& InTargets) override;

	virtual bool Test(const PCGExData::FElement& InTargetElement, const TSharedPtr<PCGExData::FPointIO>& PointIO) const override;

protected:
	TArray<TSharedPtr<PCGEx::TAttributeBroadcaster<double>>> TargetGetters;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExMatchAttrToAttrNumFactory : public UPCGExMatchRuleFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExMatchAttrToAttrNumConfig Config;

	virtual TSharedPtr<FPCGExMatchRuleOperation> CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="DataMatch", meta=(PCGExNodeLibraryDoc="misc/data-matching/match-rule"))
class UPCGExCreateMatchAttrToAttrNumSettings : public UPCGExMatchRuleFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		MatchAttrToAttr, "Match : Attributes (Numeric)", "Compares target attribute value to candidate @Data domain value",
		FName(GetDisplayName()))

#endif
	//~End UPCGSettings

	/** Rules properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExMatchAttrToAttrNumConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

protected:
	virtual bool IsCacheable() const override { return true; }

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
