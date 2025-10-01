﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMatchRuleFactoryProvider.h"
#include "PCGExMath.h"
#include "PCGExPointsProcessor.h"

#include "PCGExMatchByIndex.generated.h"

namespace PCGEx
{
	template <typename T>
	class TAttributeBroadcaster;
}

UENUM()
enum class EPCGExMatchByIndexSource : uint8
{
	Target    = 0 UMETA(DisplayName = "Target", ToolTip="Reads the specific index value on the target and compares it against the index of the input candidate"),
	Candidate = 1 UMETA(DisplayName = "Candidate", ToolTip="Reads the specific index value on the input candidate and compares it against the index of the target"),
};

USTRUCT(BlueprintType)
struct FPCGExMatchByIndexConfig : public FPCGExMatchRuleConfigBase
{
	GENERATED_BODY()

	FPCGExMatchByIndexConfig() :
		FPCGExMatchRuleConfigBase()
	{
		IndexAttribute.Update(TEXT("$Index"));
	}

	/** The attribute to read on the candidates (the data that's not used as target). Only support @Data domain, and will only try to read from there. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExMatchByIndexSource Source = EPCGExMatchByIndexSource::Target;

	/** The attribute to read on the candidates (the data that's not used as target). Only support @Data domain, and will only try to read from there. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector IndexAttribute;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Tile;

	virtual void Init() override;
};

/**
 * 
 */
class FPCGExMatchByIndex : public FPCGExMatchRuleOperation
{
public:
	FPCGExMatchByIndexConfig Config;

	virtual bool PrepareForTargets(FPCGExContext* InContext, const TSharedPtr<TArray<PCGExData::FTaggedData>>& InTargets) override;
	virtual bool Test(const PCGExData::FConstPoint& InTargetElement, const TSharedPtr<PCGExData::FPointIO>& PointIO, const PCGExMatching::FMatchingScope& InMatchingScope) const override;

protected:
	TArray<TSharedPtr<PCGEx::TAttributeBroadcaster<int32>>> IndexGetters;
	bool bIsIndex = false;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExMatchByIndexFactory : public UPCGExMatchRuleFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExMatchByIndexConfig Config;

	virtual bool WantsPoints() override;

	virtual TSharedPtr<FPCGExMatchRuleOperation> CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="DataMatch", meta=(PCGExNodeLibraryDoc="misc/data-matching/index"))
class UPCGExCreateMatchByIndexSettings : public UPCGExMatchRuleFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		MatchByIndex, "Match : By Index", "Match by index",
		FName(GetDisplayName()))

#endif
	//~End UPCGSettings

	/** Rules properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExMatchByIndexConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

protected:
	virtual bool IsCacheable() const override { return true; }

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
